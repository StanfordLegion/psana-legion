# DO NOT call this file directly.
# Instead, copy and paste this command into your shell.

# Also copy bbf.conf into the current directory.

salloc \
--job-name=psana_legion_bb \
--dependency=singleton \
--nodes=17 \
--time=00:30:00 \
--partition=debug \
--constraint=knl,quad,cache \
--core-spec=4 \
--image=docker:stanfordlegion/psana-legion:latest \
--exclusive \
--mail-type=ALL \
--account=lcls \
--bbf="bbf.conf"
