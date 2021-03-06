/*
 * opencog/learning/moses/optimization/hill-climbing.cc
 *
 * Copyright (C) 2002-2008 Novamente LLC
 * Copyright (C) 2012 Poulin Holdings
 * All Rights Reserved
 *
 * Written by Moshe Looks
 *            Predrag Janicic
 *            Nil Geisweiller
 *            Xiaohui Liu
 *            Linas Vepstas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <math.h>   // for sqrtf, cbrtf

#include <opencog/util/oc_omp.h>

#include "../moses/neighborhood_sampling.h"

#include "hill-climbing.h"

namespace opencog { namespace moses {


///////////////////
// Hill Climbing //
///////////////////

unsigned hill_climbing::operator()(deme_t& deme,
                    const instance& init_inst,
                    const iscorer_base& iscorer, unsigned max_evals,
                    unsigned* eval_best)
{
    logger().debug("Local Search Optimization");
    logger().info() << "Demes: # "   /* Legend for graph stats */
            "deme_count\t"
            "iteration\t"
            "total_steps\t"
            "total_evals\t"
            "microseconds\t"
            "new_instances\t"
            "num_instances\t"
            "num_evals\t"
            "has_improved\t"
            "best_weighted_score\t"
            "delta_weighted\t"
            "best_raw\t"
            "delta_raw\t"
            "complexity";


    // Collect statistics about the run, in struct optim_stats
    nsteps = 0;
    deme_count ++;
    over_budget = false;
    struct timeval start;
    gettimeofday(&start, NULL);

    // Initial eval_best in case nothing is found.
    if (eval_best)
        *eval_best = 0;

    const field_set& fields = deme.fields();

    // Estimate the number of nearest neighbors.
    size_t nn_estimate = information_theoretic_bits(fields);
    field_set_size = nn_estimate;  // optim stats, printed by moses.

    size_t current_number_of_evals = 0;
    size_t current_number_of_instances = 0;

    unsigned max_distance = opt_params.max_distance(fields);

    // center_inst is the current location on the hill.
    // Copy it, don't reference it, since sorting will mess up a ref.
    instance center_inst(init_inst);
    composite_score best_cscore = worst_composite_score;
    score_t best_score = very_worst_score;
    score_t best_raw_score = very_worst_score;

    // Initial distance is zero, so that the first time through
    // the loop, we handle just one instance, the initial instance.
    // (which is at distance zero from itself, of course).
    unsigned distance = 0;
    unsigned iteration = 0;

    // Needed for correlated neighborhood exploration.
    instance prev_center = center_inst;
    size_t prev_start = 0;
    size_t prev_size = 0;

    bool rescan = false;
    bool last_chance = false;

    // Whether the score has improved during an iteration
    while (true)
    {
        ++iteration;
        logger().debug("Iteration: %u", iteration);

        // Estimate the number of neighbours at the distance d.
        // This is faster than actually counting.
        size_t total_number_of_neighbours;

        // Number of instances to try, this go-around.
        size_t number_of_new_instances;

        // For a distance of one, we use
        // information_theoretic_bits as an approximation of the
        // number of neighbors, over-estimated by a factor of 2,
        // just to decrease the chance of going into 'sample' mode
        // when working with the deme. This saves cpu time.
        if (distance == 1)
        {
            total_number_of_neighbours = 2*nn_estimate;
            number_of_new_instances = 2*nn_estimate;

            // fraction_of_nn is 1 by default
            number_of_new_instances *= hc_params.fraction_of_nn;

            // Clamp the number of nearest-neighbor evaluations
            // we'll do.  This is necessitated because some systems
            // have a vast number of nearest neighbors, and searching
            // them all explodes the RAM usage, for no gain.
            if (number_of_new_instances > hc_params.max_nn_evals)
                number_of_new_instances = hc_params.max_nn_evals;

            // avoid overflow.
            size_t nleft =
                max_evals - current_number_of_evals;
            if (nleft < number_of_new_instances)
                number_of_new_instances = nleft;

        }
        else if (distance == 0)
        {
            total_number_of_neighbours = 1;
            number_of_new_instances = 1;
        }
        else // distance two or greater
        {
            // Distances greater than 1 occurs only when the -L1 flag
            // is used.  This puts this algo into a very different mode
            // of operation, in an attempt to overcome deceptive scoring
            // functions.

            // For large-distance searches, there is a combinatorial
            // explosion of the size of the search volume. Thus, be
            // careful budget our available cycles.
            total_number_of_neighbours =
                safe_binomial_coefficient(nn_estimate, distance);

            number_of_new_instances = total_number_of_neighbours;

            // binomial coefficient has a combinatoric explosion to
            // the power distance. So throttle back by fraction raised
            // to power dist.
            for (unsigned k=0; k<distance; k++)
               number_of_new_instances *= hc_params.fraction_of_nn;

            // Clamp the number of nearest-neighbor evaluations
            // we'll do.  This is necessitated because some systems
            // have a vast number of nearest neighbors, and searching
            // them all explodes the RAM usage, for no gain.
            if (number_of_new_instances > hc_params.max_nn_evals)
                number_of_new_instances = hc_params.max_nn_evals;

            size_t nleft =
                max_evals - current_number_of_evals;

// we choose the number 100 because below that multithreading is
// disabled and it leads to some massive slow down because then most
// of the computational power is spent on successive representation
// building
#define MINIMUM_DEME_SIZE         100

            // If fraction is small, just use up the rest of the cycles.
            if (number_of_new_instances < MINIMUM_DEME_SIZE)
                number_of_new_instances = nleft;

            // avoid overflow.
            if (nleft < number_of_new_instances)
                number_of_new_instances = nleft;

        }
        logger().debug(
            "Budget %u samples out of estimated %u neighbours",
            number_of_new_instances, total_number_of_neighbours);

        // The first few times through, (or, if we decided on a
        // full rescan), explore the entire nearest neighborhood.
        // Otherwise, make the optimistic assumption that the best
        // the best new instances are likely to be genric cross-overs
        // of the current top-scoring instances.  This assumption
        // seems to work really quite well.
        //
        // Based on experiments, crossing over the 40 highest-scoring
        // instances seems to offer plenty of luck, to TOP_POP_SIZE
        // is currently defined to be 40.
        //
        // If the size of the nearest neighborhood is small enough,
        // then don't bother with the optimistic guessing.  The
        // cross-over guessing will generate 3*TOP_POP_SIZE
        // instances.  Our guestimate for number of neighbors
        // intentionally over-estimates by a factor of two. So the
        // breakeven point would be 6*TOP_POP_SIZE, and we pad this
        // a bit, as small exhaustive searches do beat guessing...
#define TOP_POP_SIZE 40
        if (!hc_params.crossover || (iteration <= 2) || rescan ||
            total_number_of_neighbours < 10*TOP_POP_SIZE) {

            // The current_number_of_instances arg is needed only to
            // be able to manage the size of the deme appropriately.
            number_of_new_instances =
                sample_new_instances(total_number_of_neighbours,
                                     number_of_new_instances,
                                     current_number_of_instances,
                                     center_inst, deme, distance);
        } else {
            // These cross-over (in the genetic sense) the
            // top-scoring one, two and three instances,respectively.
            number_of_new_instances =
                cross_top_one(deme, current_number_of_instances, TOP_POP_SIZE,
                              prev_start, prev_size, prev_center);

            number_of_new_instances +=
                cross_top_two(deme,
                              current_number_of_instances + number_of_new_instances,
                              TOP_POP_SIZE,
                              prev_start, prev_size, prev_center);

            number_of_new_instances +=
                cross_top_three(deme,
                              current_number_of_instances + number_of_new_instances,
                              TOP_POP_SIZE,
                              prev_start, prev_size, prev_center);
        }
        prev_start = current_number_of_instances;
        prev_size = number_of_new_instances;
        prev_center = center_inst;

        logger().debug("Evaluate %u neighbors at distance %u",
                       number_of_new_instances, distance);

        // score all new instances in the deme
        OMP_ALGO::transform
            (next(deme.begin(), current_number_of_instances), deme.end(),
             next(deme.begin_scores(), current_number_of_instances),
             // using bind cref so that score is passed by
             // ref instead of by copy
             boost::bind(boost::cref(iscorer), _1));

        // Check if there is an instance in the deme better than
        // the best candidate
        score_t prev_hi = best_score;
        score_t prev_best_raw = best_raw_score;

        unsigned ibest = current_number_of_instances;
        for (unsigned i = current_number_of_instances;
             deme.begin() + i != deme.end(); ++i) {
            const composite_score &inst_cscore = deme[i].second;
            score_t iscore = get_penalized_score(inst_cscore);
            if (iscore >  best_score) {
                best_cscore = inst_cscore;
                best_score = iscore;
                ibest = i;
            }

            // The instance with the best raw score will typically
            // *not* be the same as the the one with the best
            // weighted score.  We need the raw score for the
            // termination condition, as, in the final answer, we
            // want the best raw score, not the best weighted score.
            score_t rscore = get_score(inst_cscore);
            if (rscore >  best_raw_score) {
                best_raw_score = rscore;
            }
        }

        bool has_improved = opt_params.score_improved(best_score, prev_hi);

        // Make a copy of the best instance.
        if (has_improved)
            center_inst = deme[ibest].first;

#ifdef GATHER_STATS
        if (iteration > 1) {
            if (scores.size() < number_of_new_instances) {
                unsigned old = scores.size();
                scores.resize(number_of_new_instances);
                counts.resize(number_of_new_instances);
                for (unsigned i=old; i<number_of_new_instances; i++) {
                    scores[i] = 0.0;
                    counts[i] = 0.0;
                }
            }

            // Gather statistics: compute the average distribution
            // of scores, as compared to the previous high score.
            std::sort(next(deme.begin(), current_number_of_instances),
                      deme.end(),
                      std::greater<deme_inst_t>());
            for (unsigned i = current_number_of_instances;
                 next(deme.begin(), i) != deme.end(); ++i)
            {
                composite_score inst_cscore = deme[i].second;
                score_t iscore = get_penalized_score(inst_cscore);
                unsigned j = i - current_number_of_instances;
                scores[j] += iscore;
                counts[j] += 1.0;

                if (prev_hi < iscore) num_improved += 1.0;
            }
            hiscore += prev_hi;
            hicount += 1.0;
            count_improved += 1.0;
        }
#endif
        current_number_of_evals += number_of_new_instances;
        current_number_of_instances += number_of_new_instances;

        // Keep the size of the deme at a managable level.
        // Large populations can easily blow out the RAM on a machine,
        // so we want to keep it at some reasonably trim level.
        //
        // To avoid wasting cpu time pointlessly, don't bother with
        // population size management if its already small.
#define ACCEPTABLE_SIZE 2000
        if (ACCEPTABLE_SIZE < current_number_of_instances) {
            bool did_resize = resize_deme(deme, best_score);

            // After resizing, some of the variables set above become
            // invalid.  Some are not needed any more: e.g. ibest.
            // Others, we need, esp prev_start and prev_size for the
            // cross-over to work.
            if (did_resize) {
                current_number_of_instances = deme.size();
                prev_start = 0;
                prev_size = current_number_of_instances;
            }
        }

        if (has_improved) {
            distance = 1;
            if (eval_best)
                *eval_best = current_number_of_instances;

            if (logger().isDebugEnabled()) {
                logger().debug() << "Best score: " << best_cscore;
                if (logger().isFineEnabled()) {
                    logger().fine() << "Best instance: "
                                    << fields.stream(center_inst);
                }
            }
        }
        else
            distance++;

        // Collect statistics about the run, in struct optim_stats
        nsteps ++;
        total_steps ++;
        total_evals += number_of_new_instances;
        struct timeval stop, elapsed;
        gettimeofday(&stop, NULL);
        timersub(&stop, &start, &elapsed);
        start = stop;
        unsigned usec = 1000000 * elapsed.tv_sec + elapsed.tv_usec;

        // Deme statistics, for performance graphing.
        if (logger().isInfoEnabled()) {
            logger().info() << "Demes: "
                << deme_count << "\t"
                << iteration << "\t"
                << total_steps << "\t"
                << total_evals << "\t"
                << usec << "\t"
                << number_of_new_instances << "\t"
                << current_number_of_instances << "\t"
                << current_number_of_evals << "\t"
                << has_improved << "\t"
                << best_score << "\t"   /* weighted score */
                << best_score - prev_hi << "\t"  /* previous weighted */
                << best_raw_score << "\t"     /* non-weighted, raw score */
                << best_raw_score - prev_best_raw << "\t"
                << get_complexity(best_cscore);
        }

        /* If this is the first time through the loop, then
         * distance was zero, there was only one instance at
         * dist=0, and we just scored it. Be sure to go around and
         * do at least the distance == 1 nearest-neighbor
         * exploration.  Note that it is possible to have only 1
         * neighbor with a distance greater than 0 (when the
         * distance has reached the deme dimension), which is why
         * we check that distance == 1 as well.
         */
        if (number_of_new_instances == 1 && distance == 1) continue;

        /* If we've blown our budget for evaluating the scorer,
         * then we are done. */
        if (max_evals <= current_number_of_evals) {
            over_budget = true;
            logger().debug("Terminate Local Search: Over budget");
            break;
        }

        /* If we've aleady gotten the best possible score, we are done. */
        if (opt_params.terminate_if_gte <= best_raw_score) {
            logger().debug("Terminate Local Search: Found best score");
            break;
        }

        if (hc_params.crossover) {
            /* If the score hasn't taken a big step recently, then
             * re-survey the immediate local neighborhood.  We may get
             * lucky.
             */
            bool big_step = opt_params.score_improved(best_score, prev_hi);
            if (!big_step && !last_chance) {

                /* If we've been using the simplex extrapolation
                 * (which is the case when 2<iteration), and there's
                 * been no improvement, then try a full nearest-
                 * neighborhood scan.  This tends to refresh the pool
                 * of candidates, and keep things going a while longer.
                 */
                if (!rescan && (2 < iteration)) {
                    rescan = true;
                    distance = 1;
                    continue;
                }

                /* If we just did the nearest neighbors, and found no
                 * improvement, then try again with the simplexes.  That's
                 * cheap & quick and one last chance to get lucky ...
                 */
                if (rescan || (2 == iteration)) {
                    rescan = false;
                    last_chance = true;
                    distance = 1;
                    continue;
                }
            }

            /* Reset back to normal mode. */
            has_improved = big_step;
            rescan = false;
            last_chance = false;
        }

        /* If we've widened the search out to the max distance, we're done. */
        if (max_distance < distance) {
            logger().debug("Terminate Local Search: Max search distance exceeded");
            break;
        }

        /* If things haven't improved, we must be at the top of the hill.
         * Terminate, unless we've been asked to widen the search.
         * (i.e. to search for other nearby hills) */
        if (!has_improved && !hc_params.widen_search) {
            logger().debug("Terminate Local Search: No improvement");
            break;
        }

        /* If we're in single-step mode, then exit the loop as soon
         * as we find improvement. */
        if (hc_params.single_step && has_improved) {
            logger().debug("Terminate Local Search: Single-step and found improvment");
            break;
        }
    }

    return current_number_of_evals;
}

size_t hill_climbing::cross_top_one(deme_t& deme,
                          size_t deme_size,
                          size_t num_to_make,
                          size_t sample_start,
                          size_t sample_size,
                          const instance& base)
{
    OC_ASSERT (sample_size > 0, "Cross-over sample size must be positive");
    if (sample_size-1 < num_to_make) num_to_make = sample_size-1;

    // We need to access the high-scorers.
    // We don't actually need them all sorted; we only need
    // the highest-scoring num_to_make+1 to appear first, and
    // after that, we don't care about the ordering.
    // std::sort(next(deme.begin(), sample_start),
    //          next(deme.begin(), sample_start + sample_size),
    //          std::greater<deme_inst_t>());
    std::partial_sort(next(deme.begin(), sample_start),
                      next(deme.begin(), sample_start + num_to_make+1),
                      next(deme.begin(), sample_start + sample_size),
                      std::greater<deme_inst_t>());

    deme.resize(deme_size + num_to_make);

    const instance &reference = deme[sample_start].first;

    // Skip the first entry; its the top scorer.
    for (unsigned i = 0; i< num_to_make; i++) {
        unsigned j = deme_size + i;
        deme[j] = deme[sample_start + i + 1]; // +1 to skip the first one
        deme.fields().merge_instance(deme[j], base, reference);
    }
    return num_to_make;
}

/** two-dimensional simplex version of above. */
size_t hill_climbing::cross_top_two(deme_t& deme,
                          size_t deme_size,
                          size_t num_to_make,
                          size_t sample_start,
                          size_t sample_size,
                          const instance& base)
{
    // sample_size choose two.
    unsigned max = sample_size * (sample_size-1) / 2;
    if (max < num_to_make) num_to_make = max;

    // std::sort(next(deme.begin(), sample_start),
    //          next(deme.begin(), sample_start + sample_size),
    //          std::greater<deme_inst_t >());
    //
    unsigned num_to_sort = sqrtf(2*num_to_make) + 3;
    if (sample_size < num_to_sort) num_to_sort = sample_size;
    std::partial_sort(next(deme.begin(), sample_start),
                      next(deme.begin(), sample_start + num_to_sort),
                      next(deme.begin(), sample_start + sample_size),
                      std::greater<deme_inst_t >());

    deme.resize(deme_size + num_to_make);

    // Summation is over a 2-simplex
    for (unsigned i = 1; i < sample_size; i++) {
        const instance& reference = deme[sample_start+i].first;
        for (unsigned j = 0; j < i; j++) {
            unsigned n = i*(i-1)/2 + j;
            if (num_to_make <= n) return num_to_make;
            unsigned ntgt = deme_size + n;
            deme[ntgt] = deme[sample_start + j];
            deme.fields().merge_instance(deme[ntgt], base, reference);
        }
    }
    return num_to_make;
}

/** three-dimensional simplex version of above. */
size_t hill_climbing::cross_top_three(deme_t& deme,
                          size_t deme_size,
                          size_t num_to_make,
                          size_t sample_start,
                          size_t sample_size,
                          const instance& base)
{
    // sample_size choose three.
    unsigned max = sample_size * (sample_size-1) * (sample_size-2) / 6;
    if (max < num_to_make) num_to_make = max;

    // std::sort(next(deme.begin(), sample_start),
    //           next(deme.begin(), sample_start + sample_size),
    //           std::greater<deme_inst_t>());

    unsigned num_to_sort = cbrtf(6*num_to_make) + 3;
    if (sample_size < num_to_sort) num_to_sort = sample_size;
    std::partial_sort(next(deme.begin(), sample_start),
                      next(deme.begin(), sample_start + num_to_make),
                      next(deme.begin(), sample_start + sample_size),
                      std::greater<deme_inst_t>());
    deme.resize(deme_size + num_to_make);

    // Summation is over a 3-simplex
    for (unsigned i = 2; i < sample_size; i++) {
        const instance& iref = deme[sample_start+i].first;
        for (unsigned j = 1; j < i; j++) {
            const instance& jref = deme[sample_start+j].first;
            for (unsigned k = 0; k < i; k++) {
                unsigned n = i*(i-1)*(i-2)/6 + j*(j-1)/2 + k;
                if (num_to_make <= n) return num_to_make;
                unsigned ntgt = deme_size + n;
                deme[ntgt] = deme[sample_start + k];
                deme.fields().merge_instance(deme[ntgt], base, iref);
                deme.fields().merge_instance(deme[ntgt], base, jref);
            }
        }
    }
    return num_to_make;
}

/// Shrink the deme, by removing all instances with score less than
/// 'cutoff'.  This is implemented with in-place deletion of elements
/// from a vector, with at least a token attempt to delete contigous
/// regions of low scores, in one go.  It is possible that a faster
/// algorithm would be to sort first, and then delete the tail-end of
/// the vector.  But this ixsn't know ... XXX experiment with this!?
/// ... err, but right now, trimming takes a small fraction of a second,
/// so there is no rush to fis this.
size_t hill_climbing::resize_by_score(deme_t& deme, score_t cutoff)
{
    size_t ndeleted = 0;
    while (true) {
         auto first = deme.end();
         auto last = deme.end();
         size_t contig = 0;
         for (auto it = deme.begin(); it != deme.end(); it++) {
             score_t iscore = get_penalized_score(it->second);
             if (iscore <= cutoff) {
                 if (0 == contig) first = it;
                 last = it;
                 contig ++;
             } else {
                 if (0 < contig)
                     break;
             }
         }

         if (0 == contig) 
             break;

         if (last != deme.end()) last++;
         deme.erase(first, last);
         ndeleted += contig;
    }
    logger().debug() << "Trimmed "
            << ndeleted << " low scoring instances.";
    return deme.size();
}

/// Keep the size of the deme at a managable level.
/// Large populations can easily blow out the RAM on a machine,
/// so we want to keep it at some reasonably trim level.
/// XXX TODO: we should also account for the actual size of
/// an instance (say, in bytes), because pop management is a
/// cpu-timewaster if the instances are small.
//
bool hill_climbing::resize_deme(deme_t& deme, score_t best_score)
{
    bool did_resize =  false;
    // Lets see how many we might be able to trounce.
    score_t cutoff = best_score - hc_params.score_range;
    size_t bad_score_cnt = 0;

    // To find the number of bad scores, we have to look
    // at the *whole* deme.
    for (const deme_inst_t& si : deme) {
        score_t iscore = get_penalized_score(si.second);
        if (iscore <=  cutoff)
            bad_score_cnt++;
    }

    // To avoid wasting cpu time pointlessly, don't bother with
    // population size management if we don't get any bang out
    // of it.
#define DONT_BOTHER_SIZE 500
    if (DONT_BOTHER_SIZE < bad_score_cnt) {

        logger().debug() << "Will trim " << bad_score_cnt
            << " low scoring instances out of " << deme.size();
        resize_by_score(deme, cutoff);
        did_resize = true;
    }

    // Are we still too large? Whack more, if needed.
    // We want to whack only the worst scorerers, and thus
    // a partial sort up front.
    if (hc_params.max_allowed_instances < deme.size()) {
        std::partial_sort(deme.begin(),
                          next(deme.begin(), hc_params.max_allowed_instances),
                          deme.end(),
                          std::greater<deme_inst_t>());

        deme.erase(next(deme.begin(), hc_params.max_allowed_instances),
                   deme.end());
        did_resize = true;
    }
    return did_resize;
}

} // ~namespace moses
} // ~namespace opencog

