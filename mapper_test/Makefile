all:
	${LEGION_PATH}/language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

gdb:
	LAUNCHER="gdb --args" ${LEGION_PATH}/language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper
