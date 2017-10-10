all:	all_png

run:
	${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=2 -lg:safe_mapper

gdb:
	LAUNCHER="gdb --args" ${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

make.sortedlog: test.rg mapper.cc
	make run | sort -n > make.sortedlog

make.worker_${PROC}.sortedlog: make.sortedlog
	egrep "fetch.* p ${PROC}|fetch_and_analyze.* p ${PROC}|^analyze.* p ${PROC}" make.sortedlog | grep -v psana_mapper > $@

worker_timeline.png: make.worker_${PROC}.sortedlog
	python worker_gnuplot.py -l $< -p ${PROC}
	cat make.worker_${PROC}.gnuplot | gnuplot

make.taskpool_${PROC}.sortedlog: make.sortedlog
	grep stealbythief make.sortedlog | sort -n -k 6 > $@

taskpool_timeline.png: make.taskpool_${PROC}.sortedlog
	python steals_gnuplot.py -l $< -p ${PROC}
	cat make.taskpool_${PROC}.gnuplot | gnuplot

taskpool_timeline_1.png:
	@make taskpool_timeline.png PROC='1'

taskpool_timeline_2.png:
	@make taskpool_timeline.png PROC='2'

worker_timeline_3.png:
	@make worker_timeline.png PROC='3'

worker_timeline_4.png:
	@make worker_timeline.png PROC='4'

all_png: taskpool_timeline_1.png taskpool_timeline_2.png worker_timeline_3.png worker_timeline_4.png

clean:
	rm -f make.*sortedlog 
	rm -f worker*.dat worker*.gnuplot worker*png
	rm -f taskpool*.dat taskpool*.gnuplot taskpool*png
