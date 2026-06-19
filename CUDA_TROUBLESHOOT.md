# CUDA Troubleshooting

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
