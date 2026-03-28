test: tests sd_style_tests
	./tests
	./sd_style_tests

tests: tests.cpp style_parser.h
	# clang++ -std=c++11 -g -fsanitize=address -fno-omit-frame-pointer -MD -MP -o tests tests.cpp -lm
	g++ -O -g -std=c++14 -MD -MP -o tests tests.cpp -lm

sd_style_tests: sd_style_test.cpp sd_style.h
	g++ -O -g -std=c++14 -MD -MP -o sd_style_tests sd_style_test.cpp -lm

-include *.d

