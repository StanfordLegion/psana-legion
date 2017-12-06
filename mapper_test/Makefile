all:	all_jpeg

run: test.rg mapper.cc mapper.h Makefile
	${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=2 -lg:safe_mapper

debug: test.rg mapper.cc mapper.h Makefile
	${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

debug.log:
	make debug > debug.log

gdb:
	LAUNCHER="gdb --args" ${LG_RT_DIR}/../language/regent.py test.rg -ll:cpu 4 -level psana_mapper=1 -lg:safe_mapper

make.sortedlog: test.rg mapper.cc
	make run | sort -n > make.sortedlog

make.taskpool_${PROC}.sortedlog: make.sortedlog
	cat make.sortedlog | egrep "p 1d0000000000000${PROC} remapping" | sort -n -k 6 > $@

taskpool_timeline.jpeg: make.taskpool_${PROC}.sortedlog make.taskpool.xrange make.worker.xrange
	python steals_gnuplot.py -l $< -p ${PROC}
	cat make.taskpool_${PROC}.gnuplot | gnuplot

make.taskpool.xrange:  make.sortedlog
	rm -f make.taskpool.xmin* make.taskpool.xmax*
	make make.taskpool_1.sortedlog PROC="1"
	make make.taskpool_2.sortedlog PROC="2"
	head -1 make.taskpool_1.sortedlog >> make.taskpool.xmin.tmp
	head -1 make.taskpool_2.sortedlog >> make.taskpool.xmin.tmp
	tail -1 make.taskpool_1.sortedlog >> make.taskpool.xmax.tmp
	tail -1 make.taskpool_2.sortedlog >> make.taskpool.xmax.tmp
	cat make.taskpool.xmin.tmp | sort -n -k 6 | head -1 > make.taskpool.xmin
	cat make.taskpool.xmax.tmp | sort -n -k 6 | tail -1 > make.taskpool.xmax
	rm -f make.taskpool*tmp

make.worker_${PROC}.sortedlog: make.sortedlog
	egrep "p ${PROC} .* taskWorkloadSize" make.sortedlog | sort -n -k 5 > $@

worker_timeline.jpeg: make.worker_${PROC}.sortedlog make.worker.xrange make.taskpool.xrange
	python worker_gnuplot.py -l $< -p ${PROC}
	cat make.worker_${PROC}.gnuplot | gnuplot

make.worker.xrange: make.sortedlog
	rm -f make.worker.xmin* make.worker.xmax*
	make make.worker_1.sortedlog PROC="1"
	make make.worker_2.sortedlog PROC="2"
	make make.worker_3.sortedlog PROC="3"
	make make.worker_4.sortedlog PROC="4"
	head -1 make.worker_1.sortedlog >> make.worker.xmin.tmp
	head -1 make.worker_2.sortedlog >> make.worker.xmin.tmp
	head -1 make.worker_3.sortedlog >> make.worker.xmin.tmp
	head -1 make.worker_4.sortedlog >> make.worker.xmin.tmp
	tail -1 make.worker_1.sortedlog >> make.worker.xmax.tmp
	tail -1 make.worker_2.sortedlog >> make.worker.xmax.tmp
	tail -1 make.worker_3.sortedlog >> make.worker.xmax.tmp
	tail -1 make.worker_4.sortedlog >> make.worker.xmax.tmp
	cat make.worker.xmin.tmp | sort -n | head -1 > make.worker.xmin
	cat make.worker.xmax.tmp | sort -n | tail -1 > make.worker.xmax
	rm -f make.worker*tmp

taskpool_timeline_1.jpeg:
	@make taskpool_timeline.jpeg PROC='1'

taskpool_timeline_2.jpeg:
	@make taskpool_timeline.jpeg PROC='2'

worker_timeline_1.jpeg:
	@make worker_timeline.jpeg PROC='1'

worker_timeline_2.jpeg:
	@make worker_timeline.jpeg PROC='2'

worker_timeline_3.jpeg:
	@make worker_timeline.jpeg PROC='3'

worker_timeline_4.jpeg:
	@make worker_timeline.jpeg PROC='4'

all_jpeg: taskpool_timeline_1.jpeg taskpool_timeline_2.jpeg worker_timeline_1.jpeg worker_timeline_2.jpeg worker_timeline_3.jpeg worker_timeline_4.jpeg
#	@make almost_clean

almost_clean:
	rm -f make.*sortedlog 
	rm -f make.worker*.dat make.worker*.gnuplot 
	rm -f make.taskpool*.dat make.taskpool*.gnuplot 
	rm -f make.*.xmin make.*.xmax

clean:	almost_clean
	rm -f worker*jpeg taskpool*jpeg

