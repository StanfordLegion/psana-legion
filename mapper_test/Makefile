all:
	${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

gdb:
	LAUNCHER="gdb --args" ${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper
