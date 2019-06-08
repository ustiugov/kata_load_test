
//Open Source License.
//
//Copyright 2019 Ecole Polytechnique Federale Lausanne (EPFL)
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <math.h>


#include <lancet/stats.h>
#include <lancet/agent.h>
#include <lancet/error.h>
#include <lancet/manager.h>
#include <lancet/timestamping.h>

#define heta 1.96 // for gamma = 0.95
#define ca 1.858 // for a = 0.001

static __thread union stats *thread_stats;
static __thread struct tx_samples tx_s;
static __thread uint32_t per_thread_lat_count;
static int per_thread_samples;
static double sampling_rate;
static union stats *all_stats[64];
static struct tx_samples *all_tx[64];
static int agent_count = 0;
static uint64_t reference_ia[REFERENCE_IA_SIZE];
static struct rand_gen *reference_ia_gen;

void init_reference_ia_dist(struct rand_gen *gen)
{
	reference_ia_gen = gen;
}

void set_reference_load(uint32_t load)
{
	set_avg(reference_ia_gen, 1e6 /(double)load);
}

void set_per_thread_samples(int samples, double sr)
{
	assert(samples <= MAX_PER_THREAD_SAMPLES);
	assert(samples > 0);
	per_thread_samples = samples;

	sampling_rate = sr / 100;
}

void clear_stats(union stats *stats)
{
	switch (get_agent_type()) {
		case THROUGHPUT_AGENT:
			bzero(stats, sizeof(struct throughput_stats));
			break;
		case LATENCY_AGENT:
		case SYMMETRIC_NIC_TIMESTAMP_AGENT:
		case SYMMETRIC_AGENT:
			bzero(stats, sizeof(struct latency_stats));
			break;
		default:
			lancet_fprintf(stderr, "Unknonw agent type\n");
	}
}

void clear_all_stats(void)
{
	int i;

	for (i=0;i<agent_count;i++) {
		clear_stats(all_stats[i]);
		// clear tx samples too
		all_tx[i]->count = 0;
	}
}

static int long_compare(const void *arg1, const void *arg2)
{
	return (*(const uint64_t *)arg1 - *(const uint64_t *)arg2);
}

static int sample_cmp_latency(const void *arg1, const void *arg2)
{
	const struct lat_sample *lts1, *lts2;

	lts1 = (const struct lat_sample *)arg1;
	lts2 = (const struct lat_sample *)arg2;

	return lts1->nsec - lts2->nsec;
}

static int sample_cmp_tx(const void *arg1, const void *arg2)
{
	const struct lat_sample *lts1, *lts2;

	lts1 = (const struct lat_sample *)arg1;
	lts2 = (const struct lat_sample *)arg2;

	if (lts1->tx.tv_sec != lts2->tx.tv_sec)
		return lts1->tx.tv_sec > lts2->tx.tv_sec;

	return lts1->tx.tv_nsec > lts2->tx.tv_nsec;
}

/*
 * Smirnov-Kolmogorov statistic implementation
 */
static double ks(uint64_t *a, uint64_t *b, int a_len, int b_len)
{
	int ia, ib;
	uint64_t min_val;
	double diff, max_diff;

	ia = 0;
	ib = 0;

	/* Sort the two arrays */
	qsort(a, a_len, sizeof(uint64_t), long_compare);
	qsort(b, b_len, sizeof(uint64_t), long_compare);

	max_diff = 0;
	while ((ia < a_len) && (ib < b_len)) {
		min_val = a[ia] < b[ib] ? a[ia] : b[ib];

		if (min_val == a[ia]) {
			diff = ((ia+1)/((double)a_len)) - (ib/((double)b_len));
			ia++;
		}
		if (min_val == b[ib]) {
			diff = (ib+1)/((double)b_len) - ia/((double)a_len);
			ib++;
		}
		if (diff < 0)
			diff *= (-1.0);
		if (max_diff < diff) {
			max_diff = diff;
		}
	}
	return max_diff;
}

static double ks_lts(struct lat_sample *a, struct lat_sample *b, int a_len, int b_len)
{
	int ia, ib;
	uint64_t min_val;
	double diff, max_diff;

	ia = 0;
	ib = 0;

	/* Sort the two arrays */
	qsort(a, a_len, sizeof(struct lat_sample), sample_cmp_latency);
	qsort(b, b_len, sizeof(struct lat_sample), sample_cmp_latency);

	max_diff = 0;
	while ((ia < a_len) && (ib < b_len)) {
		min_val = a[ia].nsec < b[ib].nsec ? a[ia].nsec : b[ib].nsec;

		if (min_val == a[ia].nsec) {
			diff = ((ia+1)/((double)a_len)) - (ib/((double)b_len));
			ia++;
		}
		if (min_val == b[ib].nsec) {
			diff = (ib+1)/((double)b_len) - ia/((double)a_len);
			ib++;
		}
		if (diff < 0)
			diff *= (-1.0);
		if (max_diff < diff) {
			max_diff = diff;
		}
	}
	return max_diff;
}

uint32_t compute_convergence(struct lat_sample *samples, int size)
{
	double dnm, val, size_d;
	// sort the samples based on tx timestamp
	qsort(samples, size, sizeof(struct lat_sample), sample_cmp_tx);

	dnm = ks_lts(samples, &samples[size/2], size/2, size/2);
	size_d = size/2;
	val = ca * sqrt((2*size_d)/(size_d*size_d));
	//printf("Dnm = %lf, val = %lf, size = %d\n", dnm, val, size);
	//assert(0);
#ifdef DUMP_SAMPLES
	FILE *fp;
	fp = fopen("/tmp/kogias/lancet-samples", "w");
	for (int i=0;i<size;i++)
		fprintf(fp, "%ld\n", samples[i].nsec);
	fclose(fp);
#endif

	return dnm < val;
}

void compute_latency_percentiles(struct latency_stats *lt_s)
{
	int idx;
	/* sort samples */
	assert(0);
	qsort(lt_s->samples, lt_s->size, sizeof(struct lat_sample),
			sample_cmp_latency);

	idx = lt_s->size * 50 / 100;
	lt_s->p50 = lt_s->samples[idx].nsec;
	idx = lt_s->size * 90 / 100;
	lt_s->p90 = lt_s->samples[idx].nsec;
	idx = lt_s->size * 95 / 100;
	lt_s->p95 = lt_s->samples[idx].nsec;
	idx = lt_s->size * 99 / 100;
	lt_s->p99 = lt_s->samples[idx].nsec;
}

struct ci_idx get_ci_bounds(int n, double p)
{
	struct ci_idx res;
	double prod, sq;

	prod = n*p;
	sq = heta * sqrt(prod*(1-p));

	res.i = (uint32_t)floor(prod - sq);
	res.k = (uint32_t)ceil(prod + sq) + 1;

	return res;
}

void compute_latency_percentiles_ci(struct latency_stats *lt_s)
{
	int idx, i;
	uint64_t sum=0;
	struct ci_idx bounds;
	/* sort samples */
	qsort(lt_s->samples, lt_s->size, sizeof(struct lat_sample),
			sample_cmp_latency);
	for (i=0;i<lt_s->size;i++)
		sum += lt_s->samples[i].nsec;

	assert(lt_s->size>0);
	lt_s->avg_lat = sum / lt_s->size;

	idx = lt_s->size * 50 / 100;
	lt_s->p50 = lt_s->samples[idx].nsec;
	bounds = get_ci_bounds(lt_s->size, 0.50);
	lt_s->p50_i = lt_s->samples[bounds.i].nsec;
	lt_s->p50_k = lt_s->samples[bounds.k].nsec;

	idx = lt_s->size * 90 / 100;
	lt_s->p90 = lt_s->samples[idx].nsec;
	bounds = get_ci_bounds(lt_s->size, 0.90);
	lt_s->p90_i = lt_s->samples[bounds.i].nsec;
	lt_s->p90_k = lt_s->samples[bounds.k].nsec;

	idx = lt_s->size * 95 / 100;
	lt_s->p95 = lt_s->samples[idx].nsec;
	bounds = get_ci_bounds(lt_s->size, 0.95);
	lt_s->p95_i = lt_s->samples[bounds.i].nsec;
	lt_s->p95_k = lt_s->samples[bounds.k].nsec;

	idx = lt_s->size * 99 / 100;
	lt_s->p99 = lt_s->samples[idx].nsec;
	bounds = get_ci_bounds(lt_s->size, 0.99);
	lt_s->p99_i = lt_s->samples[bounds.i].nsec;
	lt_s->p99_k = lt_s->samples[bounds.k].nsec;
}

void aggregate_throughput_stats(union stats *agg_stats)
{
	int i;

	clear_stats(agg_stats);
	for (i=0;i<agent_count;i++) {
		agg_stats->th_s.rx.bytes += all_stats[i]->th_s.rx.bytes;
		agg_stats->th_s.tx.bytes += all_stats[i]->th_s.tx.bytes;
		agg_stats->th_s.rx.reqs  += all_stats[i]->th_s.rx.reqs;;
		agg_stats->th_s.tx.reqs  += all_stats[i]->th_s.tx.reqs;
	}
}

void aggregate_latency_samples(union stats *agg_stats)
{
	int i, agg_count=0;

	clear_stats(agg_stats);
	bzero(agg_stats->lt_s.samples, AGG_SAMPLE_SIZE*sizeof(struct lat_sample));

	for (i=0;i<agent_count;i++) {
		agg_stats->lt_s.th_s.rx.bytes += all_stats[i]->lt_s.th_s.rx.bytes;
		agg_stats->lt_s.th_s.tx.bytes += all_stats[i]->lt_s.th_s.tx.bytes;
		agg_stats->lt_s.th_s.rx.reqs  += all_stats[i]->lt_s.th_s.rx.reqs;;
		agg_stats->lt_s.th_s.tx.reqs  += all_stats[i]->lt_s.th_s.tx.reqs;

		memcpy(&agg_stats->lt_s.samples[agg_count], all_stats[i]->lt_s.samples, (all_stats[i]->lt_s.size)*sizeof(struct lat_sample));
		agg_count += all_stats[i]->lt_s.size;
	}
	agg_stats->lt_s.size = agg_count;
}

static int timespec_cmp(const void *arg1, const void *arg2)
{
	struct timespec *a, *b;

	a = (struct timespec *)arg1;
	b = (struct timespec *)arg2;

	if (a->tv_sec == b->tv_sec)
		return a->tv_nsec - b->tv_nsec;
	else
		return a->tv_sec - b->tv_sec;
}

void collect_reference_ia(struct rand_gen *gen)
{
	for (int i=0;i<REFERENCE_IA_SIZE;i++)
		reference_ia[i] = lround(generate(reference_ia_gen) * 1000);
}

int check_ia(void)
{
	int i, copy_idx = 0, to_copy, ret;
	struct timespec data[MAX_THREADS*MAX_PER_THREAD_TX_SAMPLES];
	static uint64_t collected_ia[MAX_THREADS*MAX_PER_THREAD_TX_SAMPLES];
	struct timespec diff;
	double ks_result, val, collected_ia_size;

	// Collect all tx
	for (i=0;i<agent_count;i++) {
		to_copy = (all_tx[i]->count > MAX_PER_THREAD_TX_SAMPLES ? MAX_PER_THREAD_TX_SAMPLES : all_tx[i]->count);
		memcpy(&data[copy_idx], all_tx[i]->samples, to_copy*sizeof(struct timespec));
		copy_idx += to_copy;
	}
	assert(copy_idx <= MAX_THREADS*MAX_PER_THREAD_TX_SAMPLES);
	// Sort
	qsort(data, copy_idx, sizeof(struct timespec), timespec_cmp);

	// Compute inter-tx time
	for (i=1;i<copy_idx;i++) {
		ret = timespec_diff(&diff, &data[i], &data[i-1]);
		assert(ret == 0);
		if (diff.tv_sec != 0) {
			lancet_fprintf(stderr, "Big IR %ld\n", diff.tv_sec);
		}
		assert(diff.tv_sec == 0);
		collected_ia[i-1] = diff.tv_nsec;
	}

#ifdef QUALITY_EXP
	// we need to have the same threashold
	//assert(copy_idx-1 > 8000);
	//copy_idx = 8001;
#endif
	lancet_fprintf(stderr, "reference_ia_size = %d, collected_ia_size = %d\n", REFERENCE_IA_SIZE, copy_idx-1);
	ks_result = ks(reference_ia, collected_ia, REFERENCE_IA_SIZE, copy_idx-1);
	collected_ia_size = copy_idx - 1;
	val = ca * sqrt((REFERENCE_IA_SIZE+collected_ia_size)/(REFERENCE_IA_SIZE*collected_ia_size));
	lancet_fprintf(stderr, "IA KS = %lf, val = %lf, pass = %d\n", ks_result, val, ks_result < val);
	return ks_result < val;
#if 0
	for (i=1;i<REFERENCE_IA_SIZE;i++)
		lancet_fprintf(stderr, "reference: %ld\n", reference_ia[i]);
	for (i=1;i<copy_idx-1;i++)
		lancet_fprintf(stderr, "collected: %ld\n", collected_ia[i]);
	assert(0);
#endif
}

double check_iid(struct latency_stats *lt_s)
{
	uint64_t sum, sum_of_squares;
	int i;
	double avg_x, avg_y, sx, sy, avg_sqr_x, avg_sqr_y, cov_sum, cov, p_corr;

	// Sort based on timestamp
	qsort(lt_s->samples, lt_s->size, sizeof(struct lat_sample), sample_cmp_tx);

	sum = 0;
	sum_of_squares = 0;
	for (i=1;i<lt_s->size-1;i++) {
		sum += lt_s->samples[i].nsec;
		sum_of_squares += (lt_s->samples[i].nsec * lt_s->samples[i].nsec);
	}
	avg_x = (sum + lt_s->samples[0].nsec) / (double) (lt_s->size-1);
	avg_y = (sum + lt_s->samples[lt_s->size-1].nsec) / (double) (lt_s->size-1);
	avg_sqr_x = (sum_of_squares + lt_s->samples[0].nsec*lt_s->samples[0].nsec)
		/ (double) (lt_s->size-1);
	avg_sqr_y = (sum_of_squares + lt_s->samples[lt_s->size-1].nsec
			*lt_s->samples[lt_s->size-1].nsec) / (double) (lt_s->size - 1);

	sx = sqrt(avg_sqr_x - avg_x*avg_x);
	sy = sqrt(avg_sqr_y - avg_y*avg_y);

	// Compute the covariance
	cov_sum = 0;
	for (i=0;i<lt_s->size-1;i++)
		cov_sum += ((lt_s->samples[i].nsec - avg_x) * (lt_s->samples[i+1].nsec - avg_y));

	cov = cov_sum / (lt_s->size-1);
	p_corr = cov / (sx*sy);
	lancet_fprintf(stderr, "Pearson correlation = %lf\n", p_corr);

	return p_corr;
}

int init_per_thread_stats(void)
{
	int thread_id;

	thread_stats = malloc(sizeof(union stats) +
			MAX_PER_THREAD_SAMPLES*sizeof(struct lat_sample));
	thread_id = __sync_fetch_and_add(&agent_count, 1);
	assert(thread_id < 64);
	all_stats[thread_id] = thread_stats;
	tx_s.count = 0;
	all_tx[thread_id] = &tx_s;
	per_thread_lat_count = 0;

	return 0;
}

int add_throughput_tx_sample(struct byte_req_pair tx_p)
{
	if (!should_measure())
		return 0;

	thread_stats->th_s.tx.bytes += tx_p.bytes;
	thread_stats->th_s.tx.reqs += tx_p.reqs;

	return 0;
}

int add_throughput_rx_sample(struct byte_req_pair rx_p)
{
	if (!should_measure())
		return 0;

	thread_stats->th_s.rx.bytes += rx_p.bytes;
	thread_stats->th_s.rx.reqs += rx_p.reqs;
	return 0;
}

int add_tx_timestamp(struct timespec *tx_ts)
{
	if (!should_measure())
		return 0;
	tx_s.samples[tx_s.count++ % MAX_PER_THREAD_TX_SAMPLES] = *tx_ts;

	return 0;
}

int add_latency_sample(long diff, struct timespec *tx)
{
	struct lat_sample *lts;

	if (!should_measure() || (drand48()>sampling_rate))
		return 0;
	lts = &thread_stats->lt_s.samples[thread_stats->lt_s.count++ % per_thread_samples];
	lts->nsec = diff;
	if (tx)
		lts->tx = *tx;
	thread_stats->lt_s.size = thread_stats->lt_s.count > per_thread_samples ? per_thread_samples : thread_stats->lt_s.count;

	return 0;
}
