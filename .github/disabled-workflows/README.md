# Disabled GPU workflows

The CI and end-to-end workflows in this directory require dedicated AWS GPU instances and credentials that are not
currently available. GitHub only discovers workflow files in `.github/workflows`, so these definitions are retained for
future use without allowing automatic or manual runs.

Release package creation does not require a GPU and remains active in `.github/workflows/release-package.yml`.
