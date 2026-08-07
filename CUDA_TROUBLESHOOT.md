# CUDA Troubleshooting

## CUDA error 701 - too many resources requested for launch

Symptom:

```text
CUDA error (701)-'too many resources requested for launch'
```

This usually means a kernel launch asks for more per-block resources than the GPU can provide for that compiled kernel. The most common cause is too many registers per block, but shared memory, stack/local memory, constants, launch bounds, and maximum block dimensions can also contribute.

Registers are allocated per thread. A simple first check is:

```text
registers per block = registers per thread * threads per block
```

For example:

```text
65 registers/thread * 1024 threads/block = 66560 registers/block
65 registers/thread * 256 threads/block  = 16640 registers/block
```

Many CUDA architectures have a practical per-block register limit around `65536` 32-bit registers. If the calculated value is close to that, a small compiler change can make a launch fail.

Register counts can differ between CUDA SDKs, drivers, and target GPUs because register allocation is performed by the CUDA compiler/toolchain. If the binary contains PTX for forward compatibility, the driver may JIT-compile that PTX for the runtime GPU, and the JIT compiler may choose a different register allocation than the original build. Different `nvcc`, `ptxas`, CUDA SDK, driver, target architecture, optimization flags, inlining decisions, and device library implementations can all change register usage.

When troubleshooting, check:

- The launch block size, including all dimensions: `block.x * block.y * block.z`.
- The kernel `REG` count reported by CUDA tools.
- Dynamic and static shared memory usage.
- Stack/local memory usage, which may indicate spills or large local objects.
- Whether the launch size is fixed manually or selected using an occupancy API such as `cudaOccupancyMaxPotentialBlockSize()`.
- Whether the inspected binary contains native SASS for the runtime GPU architecture or PTX that may be JIT-compiled by the driver.

To get register counts from a binary or object file:

```bash
cuobjdump --dump-resource-usage ./path/to/binary-or-object | c++filt
```

To find candidate binaries and CUDA object files in a build directory:

```bash
rg --files ./build-dir | rg '(\.cu\.o$|/alus_package/[^/]+$|/bin/[^/]+$)'
```

To list CUDA launch sites and nearby block-size definitions in source code:

```bash
rg -n "<<<|dim3 .*block|block_size|block_dim|thread_per_block|threads_per_block" . --glob "*.cu" --glob "*.cuh"
```

Small register-pressure example:

```cpp
__global__ void ExampleKernel(
    const double first_line_utc,          // kernel argument; loaded when used, double is 64-bit
    const double line_time_interval,      // kernel argument; another 64-bit value
    KernelArray<OrbitVector> vectors,     // small struct argument; fields such as pointer and size are used in registers
    KernelArray<PosVector> velocities,    // pointer and size may be kept live for bounds check and final store
    KernelArray<PosVector> positions) {   // pointer may be kept live for final store
    const auto block_id = blockIdx.x + blockIdx.y * gridDim.x;
    // block/thread/grid values and arithmetic intermediates also need registers while live.

    const auto index = block_id * blockDim.x + threadIdx.x;
    // index, comparison predicates, and address calculations contribute to register use.

    if (index >= velocities.size) {
        return;
    }

    const double time = first_line_utc + index * line_time_interval;
    // time is a 64-bit value; conversion and multiply/add temporaries may also be live.

    const double dt =
        (vectors.array[vectors.size - 1].timeMjd_ - vectors.array[0].timeMjd_) /
        static_cast<double>(vectors.size - 1);
    // dt needs pointer/index arithmetic, loaded values, subtraction/division temporaries, and the final double.

    auto pos_vel = GetPositionVelocity(time, vectors.array, vectors.size, dt);
    // inlined device functions add their own locals, arguments, struct fields, predicates, and temporaries.
    // This is often where most register pressure comes from.

    positions.array[index] = pos_vel.position;
    velocities.array[index] = pos_vel.velocity;
    // final stores need live output pointers, index/address calculations, and pos_vel fields.
}
```

The compiler does not assign one source variable to exactly one register. It allocates physical registers to all values live at the same time: source locals, kernel arguments after load, pointers, CUDA built-ins, predicates, inlined function locals, struct fields, and temporary expression results. `double` values and 64-bit pointers commonly need two 32-bit registers while live.

To create an initial register-count table from a binary:

```bash
cuobjdump --dump-resource-usage ./path/to/binary-or-object | python3 -c '
import re, sys, subprocess
current = None
for line in sys.stdin:
    m = re.match(r" Function (.*):$", line)
    if m:
        current = m.group(1)
        continue
    m = re.search(r"\bREG:(\d+)\b", line)
    if current and m:
        name = subprocess.run(["c++filt", current], text=True, stdout=subprocess.PIPE).stdout.strip()
        print(f"| `{name}` | `<source launch block>` | `{m.group(1)}` | `<REG * threads>` | `<notes>` |")
        current = None
'
```

Fill in the launch block size from the source launch site. Then calculate `Registers/block` as `REG/thread * block.x * block.y * block.z`.

Audited overview from `alus-noble-devel`, ALUs `v1.6.0-9-gbfefa878` / commit `bfefa878`, CUDA SDK `12.6 V12.6.85`:

| Kernel | Block | REG | Registers/block | Assessment |
|---|---:|---:|---:|---|
| `CalculateVelocitiesAndPositionsKernel` | `256` | `65` | `16640` | Safe now |
| `FindValidTriangles` | `512` | `73` | `37376` | Legal, possible occupancy concern |
| `ComputeExtendedAmountKernel` | `20x20 = 400` | `97` | `38800` | Legal, possible occupancy concern |
| `PatchStdDevReduction` | `32x32 = 1024` | `39` | `39936` | Legal |
| `PatchMeanReduction` | `32x32 = 1024` | `38` | `38912` | Legal |
| `ReduceComplexSingleBlock` | `1024` | `38` | `38912` | Legal |
| `ReduceSumRawIQData` | `1024` | `31` | `31744` | Legal |
| `ReduceIntensity` | `1024` | `28` | `28672` | Legal |
| `BoolImageForCoherenceProductFiltering` | `32x32 = 1024` | `10` | `10240` | Safe |

## CUDA Error 999 During Startup

Symptom:

```
CUDA error (999)-'unknown error' at .../util/cuda/src/cuda_device_init.cu:38
```

That line calls `cudaGetDeviceCount()`. If `nvidia-smi` works but ALUS still fails here, CUDA initialization may be stuck even though the GPU is visible.

Quick check:

```bash
python3 -c 'import ctypes; lib=ctypes.CDLL("libcuda.so.1"); print(lib.cuInit(0))'
```

Expected result:

```text
0
```

If it prints `999`, try resetting NVIDIA UVM:

```bash
sudo modprobe -r nvidia_uvm
sudo modprobe nvidia_uvm
```

Then retest:

```bash
python3 -c 'import ctypes; lib=ctypes.CDLL("libcuda.so.1"); print(lib.cuInit(0))'
```

If `modprobe -r nvidia_uvm` says the module is in use, check users:

```bash
sudo fuser -v /dev/nvidia*
```

Stop ALUS and other CUDA/GPU processes, then retry. If CUDA still returns `999`, reboot the host.

Note: `nvidia-smi` working does not guarantee CUDA works. `nvidia-smi` uses NVIDIA management APIs; ALUS needs CUDA runtime initialization.
