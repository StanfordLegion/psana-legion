# Legion Psana Testbed

## Instructions for Running CCTBX-tasking on Cori

 1. Build Cori binary.

    Add the following to `~/.bashrc.ext`:

    ```
    module unload PrgEnv-intel
    module load PrgEnv-gnu

    export CC=cc
    export CXX=CC
    ```

    Then reload your shell and run:

    ```
    mkdir psana_legion
    git clone https://github.com/StanfordLegion/gasnet.git
    git clone https://gitlab.com/StanfordLegion/legion.git
    git clone https://github.com/StanfordLegion/psana-legion.git

    pushd gasnet
    make CONDUIT=aries
    popd

    pushd psana-legion/psana_legion
    ./scripts/build_cori.sh
    ./scripts/copy_lib64.sh
    popd
    ```

 2. Request access to CCTBX demo data set.

    Place data in `$SCRATCH/demo_data`.

 3. Submit jobs.

    ```
    cd psana-legion/cctbx/test_scripts
    sbatch sbatch_legion_rax.sh
    ```
