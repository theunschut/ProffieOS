test: tests sd_style_tests sd_style_integration_tests
	./tests
	./sd_style_tests
	./sd_style_integration_tests

test-bench: sd_style_bench
	./sd_style_bench

tests: tests.cpp style_parser.h
	# clang++ -std=c++11 -g -fsanitize=address -fno-omit-frame-pointer -MD -MP -o tests tests.cpp -lm
	g++ -O -g -std=c++14 -MD -MP -o tests tests.cpp -lm

sd_style_tests: sd_style_test.cpp sd_style.h
	g++ -O -g -std=c++14 -MD -MP -o sd_style_tests sd_style_test.cpp -lm

sd_style_integration_tests: sd_style_integration_test.cpp sd_style.h
	g++ -O -g -std=c++14 -MD -MP -o sd_style_integration_tests sd_style_integration_test.cpp -lm

sd_style_bench: sd_style_bench.cpp sd_style.h
	g++ -O -g -std=c++14 -MD -MP -o sd_style_bench sd_style_bench.cpp -lm

-include *.d

