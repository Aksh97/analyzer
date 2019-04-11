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
#include "include/histogram.hpp"

#include <fstream>
#include <math.h>
#include <random>
#include <cstdlib>


Histogram* Histogram::histogram;

Histogram* Histogram::get_instance() {
	if (!histogram) 
		histogram = new Histogram();
	return histogram;
}

Histogram::~Histogram(){
	delete histogram;
}

/*!
 * @brief Construct parameter values for a histogram label on the fly.
 *
 */
struct hist_elem Histogram::construct_hist_elem(unsigned long label) {
	this->histo_param_lock.lock();
	struct hist_elem new_elem;
	std::default_random_engine r_generator(label);
	std::default_random_engine c_generator(label / 2);
	std::default_random_engine beta_generator(label);
#ifdef DEBUG
	logstream(LOG_DEBUG) << "(new construction) c = ";
#endif
	for (int i = 0; i < SKETCH_SIZE; i++) {
		new_elem.r[i] = gamma_dist(r_generator);
		new_elem.beta[i] = uniform_dist(beta_generator);
		new_elem.c[i] = gamma_dist(c_generator);
#ifdef DEBUG
		logstream(LOG_DEBUG) << new_elem.c[i] << " ";
#endif
	}
#ifdef DEBUG
	logstream(LOG_DEBUG) << std::endl;
#endif
	gamma_dist.reset();
	this->histo_param_lock.unlock();
	return new_elem;
}

#ifdef DEBUG
/*!
 * @brief To debug, we use this function to make sure generated values are the same as stored values. 
 */
void Histogram::comp(unsigned long label, struct hist_elem a, struct hist_elem b) {
	for (int i = 0; i < SKETCH_SIZE; i++) {
		if (a.r[i] != b.r[i]) {
			logstream(LOG_ERROR) << "LABEL["<<label<<"] r: Got " << b.r[i] << ". Expected " << a.r[i] << " at " << i <<  std::endl;
		}
		if (a.beta[i] != b.beta[i]) {
			logstream(LOG_ERROR) << "LABEL["<<label<<"] beta: Got " << b.beta[i] << ". Expected " << a.beta[i] << " at " << i <<  std::endl;
		}
		if (a.c[i] != b.c[i]) {
			logstream(LOG_ERROR) << "LABEL["<<label<<"] c: Got " << b.c[i] << ". Expected " << a.c[i] << " at " << i <<  std::endl;
		}
	}
}
#endif

/*!
 * @brief For decaying values in histogram map. And record the sketch at the file @fp.
 */
void Histogram::decay(FILE* fp) {
	this->histogram_map_lock.lock();
	this->t++;
	this->w++;
	/* Decay only when t == DECAY. */
	if (this->t >= DECAY) {
		std::map<unsigned long, double>::iterator it;
		for (it = this->histogram_map.begin(); it != this->histogram_map.end(); it++) {
			it->second *= this->powerful;
		}
		for (int i = 0; i < SKETCH_SIZE; i++) {
			this->hash[i] *= this->powerful;
		}
		this->t = 0; /* Reset this timer. */
	}
	/* Record sketch only when t == WINDOW. */
	if (this->w >= WINDOW) {
		for (int i = 0; i < SKETCH_SIZE; i++) {
			fprintf(fp,"%lu ", this->sketch[i]);
		}
		fprintf(fp, "\n");
		this->w = 0; /* Reset this timer. */
	}
	this->histogram_map_lock.unlock();
}


/*!
 * @brief Insert @label to the histogram_map if it does not exist; otherwise, update the mapped "cnt" value.
 * 
 * @base: if true, we do not update hash. We only update hash during streaming.
 * 
 * We decay every element in the histogram every DECAY updates.
 * We lock the whole operation here.
 *
 */
void Histogram::update(unsigned long label, bool base) {
	this->histogram_map_lock.lock();
	/* We add the new element or update the existing element in the histogram map. 
	 * This is done both in base and streaming part of the graph.
	 */
	std::pair<std::map<unsigned long, double>::iterator, bool> rst;
	double counter = 1;
	rst = this->histogram_map.insert(std::pair<unsigned long, double>(label, counter));
	if (rst.second == false) {
#ifdef DEBUG
		logstream(LOG_DEBUG) << "The label " << label << " is already in the map. Updating the sketch and its hash." << std::endl;
#endif
		(rst.first)->second++;
	}

	/* Now we update the hash if needed.
	 * Hash updates only happen in streaming.
	 */
	if (!base) {
		if (!MEMORY) {
			struct hist_elem generated_param = this->construct_hist_elem(label);
			for (int i = 0; i < SKETCH_SIZE; i++) {
				/* Compute the new hash value a. */
				double r = generated_param.r[i];
				double beta = generated_param.beta[i];
				double c = generated_param.c[i];
				double y = pow(M_E, log((rst.first)->second) - r * beta);
				double a = c / (y * pow(M_E, r));

				if (a < this->hash[i]) {
					this->hash[i] = a;
					this->sketch[i] = (rst.first)->first;
				}
			}
		} else {
			srand(label);
			int pos1 = rand() % PREGEN;
			int pos2 = rand() % PREGEN;

			for (int i = 0; i < SKETCH_SIZE; i++) {
				/* Compute the new hash value a. */
				double r = this->gamma_param[pos1][i];
				double beta = this->uniform_param[pos1][i];
				double c = this->gamma_param[pos2][i];
				double y = pow(M_E, log((rst.first)->second) - r * beta);
				double a = c / (y * pow(M_E, r));

				if (a < this->hash[i]) {
					this->hash[i] = a;
					this->sketch[i] = (rst.first)->first;
				}
			}
		}
	}
	this->histogram_map_lock.unlock();
	return;
}

/*!
 * @brief Create (and initialize) a sketch after the base graph has been proceed by GraphChi.
 *
 * Base graph is small. We can save some local sketch parameters for ease of coding. 
 * This function is called only once as initialization.
 * We lock the whole operation.
 *
 */
void Histogram::create_sketch() {
	this->histogram_map_lock.lock();
	if (!MEMORY) {
		/* Locally save some sketch parameters. */
		std::map<unsigned long, struct hist_elem> base_map;
		for (std::map<unsigned long, double>::iterator it = this->histogram_map.begin(); it != this->histogram_map.end(); it++) {
			unsigned long label = it->first;
			struct hist_elem new_elem = this->construct_hist_elem(label);
			base_map.insert(std::pair<unsigned long, struct hist_elem>(label, new_elem));
		}

		for (int i = 0; i < SKETCH_SIZE; i++) {
			/* Compute the hash value a. */
			std::map<unsigned long, double>::iterator histoit = this->histogram_map.begin();
			unsigned long label = histoit->first;
			std::map<unsigned long, struct hist_elem>::iterator basemapit;
			basemapit = base_map.find(label);
			if (basemapit == base_map.end()){
				logstream(LOG_ERROR) << "Label: " << label << " should exist in local base map, but it does not. " << std::endl;
				return;
			}
			struct hist_elem histo_param = basemapit->second;
			double y = pow(M_E, log(histoit->second) - histo_param.r[i] * histo_param.beta[i]);
			double a_i = histo_param.c[i] / (y * pow(M_E, histo_param.r[i]));
			unsigned long s_i = histoit->first;
			for (histoit = this->histogram_map.begin(); histoit != this->histogram_map.end(); histoit++) {
				label = histoit->first;
				basemapit = base_map.find(label);
				if (basemapit == base_map.end()){
					logstream(LOG_ERROR) << "Label: " << label << " should exist in local base map, but it does not. " << std::endl;
					return;
				}
				histo_param = basemapit->second;
				y = pow(M_E, log(histoit->second) - histo_param.r[i] * histo_param.beta[i]);
				double a = histo_param.c[i] / (y * pow(M_E, histo_param.r[i]));
				if (a < a_i) {
					a_i = a;
					s_i = histoit->first;
				}
			}
			this->sketch[i] = s_i;
			this->hash[i] = a_i;
		}
	} else {
		/* Create all Params needed for sketch construction. */
		srand(36);
		for (unsigned long i = 0; i < (unsigned long)PREGEN; i++) {
			int randomized_i = rand();
			std::default_random_engine r_generator(randomized_i);
			std::default_random_engine beta_generator(randomized_i);

			for (int j = 0; j < SKETCH_SIZE; j++) {
				this->gamma_param[i][j] = gamma_dist(r_generator);
				this->uniform_param[i][j] = uniform_dist(beta_generator);
			}
			gamma_dist.reset();
		}

		for (int i = 0; i < SKETCH_SIZE; i++) {
			std::map<unsigned long, double>::iterator histoit = this->histogram_map.begin();
			unsigned long label = histoit->first;

			srand(label);
			int pos1 = rand() % PREGEN; //For Gamma and uniform
			int pos2 = rand() % PREGEN; //For the other gamma

			double y = pow(M_E, log(histoit->second) - this->gamma_param[pos1][i] * this->uniform_param[pos1][i]);
			double a_i = this->gamma_param[pos2][i] / (y * pow(M_E, this->gamma_param[pos1][i]));
			unsigned long s_i = histoit->first;
			for (histoit = this->histogram_map.begin(); histoit != this->histogram_map.end(); histoit++) {
				label = histoit->first;

				srand(label);
				pos1 = rand() % PREGEN;
				pos2 = rand() % PREGEN;

				y = pow(M_E, log(histoit->second) - this->gamma_param[pos1][i] * this->uniform_param[pos1][i]);
				double a = this->gamma_param[pos2][i] / (y * pow(M_E, this->gamma_param[pos1][i]));
				if (a < a_i) {
					a_i = a;
					s_i = histoit->first;
				}
			}
			this->sketch[i] = s_i;
			this->hash[i] = a_i;
		}
	}
	this->histogram_map_lock.unlock();
	return;
}

void Histogram::record_sketch(FILE* fp) {
	this->histogram_map_lock.lock();
	for (int i = 0; i < SKETCH_SIZE; i++) {
		fprintf(fp,"%lu ", this->sketch[i]);
	}
	fprintf(fp, "\n");
	this->histogram_map_lock.unlock();
	return;
}

#ifdef DEBUG
/*!
 * @brief Print the histogram map for debugging.
 * Simply returns for now since it slows the program down in CircleCI.
 *
 */
void Histogram::print_histogram() {
	std::map<unsigned long, double>::iterator it;
	logstream(LOG_DEBUG) << "Printing histogram map to the console..." << std::endl;
	for (it = this->histogram_map.begin(); it != this->histogram_map.end(); it++)
		logstream(LOG_DEBUG) << "[" << it->first << "]->" << it->second << "  ";
	return;
}
#endif
