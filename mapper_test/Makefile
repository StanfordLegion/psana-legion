all:	run mapper_timeline

run:
	${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=2 -lg:safe_mapper

gdb:
	LAUNCHER="gdb --args" ${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

make.sortedlog: test.rg mapper.cc
	make run | sort -n > make.sortedlog

make.worker_${PROC}.sortedlog: make.sortedlog
	egrep "fetch.* p ${PROC}|fetch_and_analyze.* p ${PROC}|^analyze.* p ${PROC}" make.sortedlog | grep -v psana_mapper > make.worker_${PROC}.sortedlog

mapper_timeline.png: make.worker_${PROC}.sortedlog
	python mapper_gnuplot.py -l make.worker_${PROC}.sortedlog -p ${PROC}
	cat make.worker_${PROC}.gnuplot | gnuplot

mapper_timeline_3.png:
	@make mapper_timeline.png PROC='3'

mapper_timeline_4.png:
	@make mapper_timeline.png PROC='4'

mapper_timeline: make.sortedlog mapper_timeline_3.png mapper_timeline_4.png
	python mapper_timeline.py -l make.sortedlog > mapper_timeline

clean:
	rm -f mapper_timeline.dat mapper_timeline.png make.*sortedlog
