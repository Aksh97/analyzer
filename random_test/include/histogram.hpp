/*
 *
 * Author: Xueyuan Han <hanx@g.harvard.edu>
 *
 * Copyright (C) 2018 Harvard University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 */
#ifndef histogram_hpp
#define histogram_hpp

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include "def.hpp"

/* We use singleton design to create a single instance of a histogram.
 * This is not thread-safe. A proper locking mechanism is needed.
 * 
 * Current implementation uses a simple ordered Map.
 */

class Histogram {
public:
	static Histogram* get_instance();
	~Histogram();
	void update(unsigned long label, bool increment_t, bool base, std::map<unsigned long, struct hist_elem>& param_map);
	void create_sketch(std::map<unsigned long, struct hist_elem>& param_map);
	void record_sketch(FILE* fp);

private:
	static Histogram* histogram;

	Histogram() {
		this->t = 0;
	}

	std::map<unsigned long, double> histogram_map; /* We use unicorn.db for hash values. histogram_map only maps label to counter. */
	unsigned long sketch[SKETCH_SIZE];
	double hash[SKETCH_SIZE];

	int t; /* If t reaches decay, we decay the cnt and hash value by e^(-lambda).*/

	/* The lock needed for updating histogram map. */
	std::mutex histogram_map_lock;

};
#include "../histogram.cpp"

#endif