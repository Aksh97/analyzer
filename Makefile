INCFLAGS = -I/usr/local/include/ -I./src/

CPP = g++-4.9
CPPFLAGS = -std=c++11 -g -O3 $(INCFLAGS)  -fopenmp -Wall -Wno-strict-aliasing -lpthread
LINKERFLAGS = -lz
DEBUGFLAGS = -g -ggdb $(INCFLAGS)
HEADERS=$(shell find . -name '*.hpp')


all: apps tests 
apps: example_apps/connectedcomponents example_apps/pagerank example_apps/pagerank_functional example_apps/communitydetection example_apps/unionfind_connectedcomps example_apps/stronglyconnectedcomponents example_apps/trianglecounting example_apps/randomwalks example_apps/minimumspanningforest
als: example_apps/matrix_factorization/als_edgefactors  example_apps/matrix_factorization/als_vertices_inmem
tests: tests/basic_smoketest tests/bulksync_functional_test tests/dynamicdata_smoketest tests/test_dynamicedata_loader

echo:
	echo $(HEADERS)
clean:
	@rm -rf bin/*
	cd toolkits/collaborative_filtering/; make clean; cd ../../
	cd toolkits/parsers/; make clean; cd ../../
	cd toolkits/graph_analytics/; make clean; cd ../../

blocksplitter: src/preprocessing/blocksplitter.cpp $(HEADERS)
	$(CPP) $(CPPFLAGS) src/preprocessing/blocksplitter.cpp -o bin/blocksplitter $(LINKERFLAGS)

sharder_basic: src/preprocessing/sharder_basic.cpp $(HEADERS)
	@mkdir -p bin
	$(CPP) $(CPPFLAGS) src/preprocessing/sharder_basic.cpp -o bin/sharder_basic $(LINKERFLAGS)

example_apps/% : example_apps/%.cpp $(HEADERS)
	@mkdir -p bin/$(@D)
	$(CPP) $(CPPFLAGS) -Iexample_apps/ $@.cpp -o bin/$@ $(LINKERFLAGS) 

extractor/% : extractor/%.cpp $(HEADERS)
	@mkdir -p bin/$(@D)
	$(CPP) $(CPPFLAGS) -Iextractor/ $@.cpp -o bin/$@ $(LINKERFLAGS)

edebug: CPPFLAGS += -DDEBUG -g
edebug: extractor/main

streaming/% : streaming/%.cpp $(HEADERS)
	@mkdir -p bin/$(@D)
	$(CPP) $(CPPFLAGS) -Istreaming/ $@.cpp -o bin/$@ $(LINKERFLAGS)

sdebug: CPPFLAGS += -DSKETCH_SIZE=2000 -DK_HOPS=3 -DDEBUG -g
sdebug: streaming/main


myapps/% : myapps/%.cpp $(HEADERS)
	@mkdir -p bin/$(@D)
	$(CPP) $(CPPFLAGS) -Imyapps/ $@.cpp -o bin/$@ $(LINKERFLAGS)

tests/%: src/tests/%.cpp $(HEADERS)
	@mkdir -p bin/$(@D)
	$(CPP) $(CPPFLAGS) src/$@.cpp -o bin/$@	$(LINKERFLAGS)


graphlab_als: example_apps/matrix_factorization/graphlab_gas/als_graphlab.cpp
	$(CPP) $(CPPFLAGS) example_apps/matrix_factorization/graphlab_gas/als_graphlab.cpp -o bin/graphlab_als $(LINKERFLAGS)

cf:
	cd toolkits/collaborative_filtering/; bash ./test_eigen.sh; 
	if [ $$? -ne 0 ]; then exit 1; fi
	cd toolkits/collaborative_filtering/; make 
cf_test:
	cd toolkits/collaborative_filtering/; make test; 
cfd:
	cd toolkits/collaborative_filtering/; make -f Makefile.debug

parsers:
	cd toolkits/parsers/; make
parsersd:
	cd toolkits/parsers/; make -f Makefile.debug
ga:
	cd toolkits/graph_analytics/; make
ta:
	cd toolkits/text_analysis/; make

docs: */**
	doxygen conf/doxygen/doxygen.config

run_youtube_v2:
	cd streaming/analyze && mkdir -p train-data-youtube-v2
	number=0 ; while [ $$number -le 99 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/youtube_data_v2/base-youtube-v2-$$number.txt niters 10000 stream_file streaming/data/youtube_data_v2/stream-youtube-v2-$$number.txt decay 100 lambda 0.02 interval 1500 sketch_file streaming/analyze/train-data-youtube-v2/sketch-youtube-v2-$$number.txt chunkify 1 chunk_size 20 ; \
		rm -rf streaming/data/youtube_data_v2/base-youtube-v2-$$number.txt.* ; \
		rm -rf streaming/data/youtube_data_v2/base-youtube-v2-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done
	
run_attack_v2:
	cd streaming/analyze && mkdir -p test-data-youtube-v2
	number=300 ; while [ $$number -le 304 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/attack_data_v2/base-attack-v2-$$number.txt niters 10000 stream_file streaming/data/attack_data_v2/stream-attack-v2-$$number.txt decay 100 lambda 0.02 interval 1500 sketch_file streaming/analyze/test-data-youtube-v2/sketch-attack-v2-$$number.txt chunkify 1 chunk_size 20 ; \
		rm -rf streaming/data/attack_data_v2/base-attack-v2-$$number.txt.* ; \
		rm -rf streaming/data/attack_data_v2/base-attack-v2-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done

run_wget_train:
	cd streaming/analyze && mkdir -p train-data-wget
	number=0 ; while [ $$number -le 99 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/wget_data/train/base-wget-$$number.txt niters 10000 stream_file streaming/data/wget_data/train/stream-wget-$$number.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/train-data-wget/sketch-wget-$$number.txt chunkify 1 chunk_size 5 ; \
		rm -rf streaming/data/wget_data/train/base-wget-$$number.txt.* ; \
		rm -rf streaming/data/wget_data/train/base-wget-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done

run_wget_test:
	cd streaming/analyze && mkdir -p test-data-wget
	bin/streaming/main filetype edgelist file streaming/data/wget_data/test/base-wget-test.txt niters 10000 stream_file streaming/data/wget_data/test/stream-wget-test.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/test-data-wget/sketch-wget-test.txt chunkify 1 chunk_size 5
	rm -rf streaming/data/wget_data/test/base-wget-test.txt.*
	rm -rf streaming/data/wget_data/test/base-wget-test.txt_*
	bin/streaming/main filetype edgelist file streaming/data/wget_data/test/base-wget-attack.txt niters 10000 stream_file streaming/data/wget_data/test/stream-wget-attack.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/test-data-wget/sketch-wget-attack.txt chunkify 1 chunk_size 5
	rm -rf streaming/data/wget_data/test/base-wget-attack.txt.*
	rm -rf streaming/data/wget_data/test/base-wget-attack.txt_*

run_ruby_train:
	cd streaming/analyze && mkdir -p train-data-ruby
	number=0 ; while [ $$number -le 8 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/ruby_data/base-ruby-$$number.txt niters 10000 stream_file streaming/data/ruby_data/stream-ruby-$$number.txt decay 25 lambda 0.02 interval 25 sketch_file streaming/analyze/train-data-ruby/sketch-ruby-$$number.txt chunkify 1 chunk_size 2 ; \
		rm -rf streaming/data/ruby_data/base-ruby-$$number.txt.* ; \
		rm -rf streaming/data/ruby_data/base-ruby-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done

run_ruby_test:
	cd streaming/analyze && mkdir -p test-data-ruby
	bin/streaming/main filetype edgelist file streaming/data/ruby_data/base-ruby-attack.txt niters 10000 stream_file streaming/data/ruby_data/stream-ruby-attack.txt decay 25 lambda 0.02 interval 25 sketch_file streaming/analyze/test-data-ruby/sketch-ruby-attack.txt chunkify 1 chunk_size 2
	rm -rf streaming/data/ruby_data/base-ruby-attack.txt.*
	rm -rf streaming/data/ruby_data/base-ruby-attack.txt_*

run_wget_train_multilabel:
	cd streaming/analyze && mkdir -p train-data-wget-multilabel
	number=0 ; while [ $$number -le 99 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/multilabel/wget_data_multilabel/train/base-wget-$$number.txt niters 10000 stream_file streaming/data/multilabel/wget_data_multilabel/train/stream-wget-$$number.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/train-data-wget-multilabel/sketch-wget-$$number.txt chunkify 1 chunk_size 5 ; \
		rm -rf streaming/data/multilabel/wget_data_multilabel/train/base-wget-$$number.txt.* ; \
		rm -rf streaming/data/multilabel/wget_data_multilabel/train/base-wget-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done

run_wget_test_multilabel:
	cd streaming/analyze && mkdir -p test-data-wget-multilabel
	number=0 ; while [ $$number -le 1 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/multilabel/wget_data_multilabel/test/base-wget-attack-$$number.txt niters 10000 stream_file streaming/data/multilabel/wget_data_multilabel/test/stream-wget-attack-$$number.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/test-data-wget-multilabel/sketch-wget-attack-$$number.txt chunkify 1 chunk_size 5 ; \
		rm -rf streaming/data/multilabel/wget_data_multilabel/test/base-wget-attack-$$number.txt.* ; \
		rm -rf streaming/data/multilabel/wget_data_multilabel/test/base-wget-attack-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done

run_background_train_multilabel:
	cd streaming/analyze && mkdir -p train-data-background-multilabel
	number=0 ; while [ $$number -le 99 ] ; do \
		bin/streaming/main filetype edgelist file streaming/data/multilabel/background_data_multilabel/base_train/base-background-$$number.txt niters 10000 stream_file streaming/data/multilabel/background_data_multilabel/stream_train/stream-background-$$number.txt decay 100 lambda 0.02 interval 200 sketch_file streaming/analyze/train-data-background-multilabel/sketch-background-$$number.txt chunkify 1 chunk_size 5 ; \
		rm -rf streaming/data/multilabel/background_data_multilabel/base_train/base-background-$$number.txt.* ; \
		rm -rf streaming/data/multilabel/background_data_multilabel/base_train/base-background-$$number.txt_* ; \
		number=`expr $$number + 1` ; \
	done


	
