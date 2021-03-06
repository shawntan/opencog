/** metapopulation.h ---
 *
 * Copyright (C) 2010 Novemente LLC
 * Copyright (C) 2012 Poulin Holdings
 *
 * Authors: Nil Geisweiller, Moshe Looks, Linas Vepstas
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


#ifndef _OPENCOG_METAPOPULATION_H
#define _OPENCOG_METAPOPULATION_H

#include <atomic>
#include <mutex>

#include <boost/unordered_set.hpp>
#include <boost/logic/tribool.hpp>

#include <opencog/comboreduct/combo/combo.h>
#include <opencog/comboreduct/reduct/reduct.h>

#include "../representation/instance_set.h"
#include "../representation/representation.h"
#include "feature_selector.h"
#include "scoring.h"
#include "types.h"

#define EVALUATED_ALL_AVAILABLE 1234567

namespace opencog {
namespace moses {

using std::pair;
using std::make_pair;
using boost::logic::tribool;
using boost::logic::indeterminate;
using namespace combo;

static const operator_set empty_ignore_ops = operator_set();

/**
 * parameters about deme management
 */
struct metapop_parameters
{
    metapop_parameters(int _max_candidates = -1,
                       bool _reduce_all = true,
                       bool _revisit = false,
                       bool _include_dominated = true,
                       score_t _complexity_temperature = 3.0f,
                       const operator_set& _ignore_ops = empty_ignore_ops,
                       // bool _enable_cache = false,    // adaptive_cache
                       unsigned _cache_size = 100000,     // is disabled
                       unsigned _jobs = 1,
                       const combo_tree_ns_set* _perceptions = NULL,
                       const combo_tree_ns_set* _actions = NULL,
                       const feature_selector* _fstor = NULL) :
        max_candidates(_max_candidates),
        reduce_all(_reduce_all),
        revisit(_revisit),
        include_dominated(_include_dominated),
        diversity_pressure(0.0),
        diversity_exponent(-1.0), // max
        diversity_p_norm(2.0),    // Euclidean
        keep_bscore(false),
        complexity_temperature(_complexity_temperature),
        cap_coef(50),
        ignore_ops(_ignore_ops),
        // enable_cache(_enable_cache),   // adaptive_cache
        cache_size(_cache_size),          // is disabled
        jobs(_jobs),
        perceptions(_perceptions),
        actions(_actions),
        merge_callback(NULL),
        callback_user_data(NULL),
        fstor(_fstor),
        linear_contin(true)
        {}

    // The max number of candidates considered to be added to the
    // metapopulation, if negative then all candidates are considered.
    int max_candidates;

    // If true then all candidates are reduced before evaluation.
    bool reduce_all;

    // When true then visited exemplars can be revisited.
    bool revisit;

    // Ignore behavioral score domination when merging candidates in
    // the metapopulation.  Keeping dominated candidates improves
    // performance by avoiding local maxima.
    bool include_dominated;

    // Diversity pressure of to enforce diversification of the
    // metapop. 0 means no diversity pressure, the higher the value
    // the strong the pressure.
    score_t diversity_pressure;

    // exponent of the generalized mean used to aggregate the
    // diversity penalties of a candidate between a set of
    // candidates. If the exponent is negative (default) then the max
    // is used instead (generalized mean with infinite exponent).
    score_t diversity_exponent;

    // Parameter value of the p-norm used to compute the distance
    // between 2 bscores
    score_t diversity_p_norm;

    // keep track of the bscores even if not needed (in case the user
    // wants to keep them around)
    bool keep_bscore;
    
    // Boltzmann temperature ...
    score_t complexity_temperature;

    // The metapopulation size is capped according to the following
    // formula:
    //
    // cap = cap_coef*(x+250)*(1+2*exp(-x/500))
    //
    // where x is the number of generations so far
    double cap_coef;
    
    // the set of operators to ignore
    operator_set ignore_ops;

    // Enable caching of scores.
    // bool enable_cache;   // adaptive_cache
    unsigned cache_size;    // is disabled

    // Number of jobs for metapopulation maintenance such as merging
    // candidates to the metapopulation.
    unsigned jobs;

    // the set of perceptions of an optional interactive agent
    const combo_tree_ns_set* perceptions;
    // the set of actions of an optional interactive agent
    const combo_tree_ns_set* actions;

    bool (*merge_callback)(bscored_combo_tree_set&, void*);
    void *callback_user_data;

    const feature_selector* fstor;

    // Build only linear expressions involving contin features.
    // This can greatly decrease the number of knobs created during
    // representation building, resulting in much smaller field sets,
    // and instances that can be searched more quickly. However, in
    // order to fit the data, linear expressions may not be as good,
    // and thus may require more time overall to find...
    bool linear_contin;
};

void print_stats_header (optim_stats *os);

struct deme_expander
{
    typedef deme_t::iterator deme_it;
    typedef deme_t::const_iterator deme_cit;

    deme_expander(const type_tree& type_signature,
                  const reduct::rule& si_ca,
                  const reduct::rule& si_kb,
                  const cscore_base& sc,
                  optimizer_base& opt,
                  const metapop_parameters& pa = metapop_parameters()) :
        _rep(NULL), _deme(NULL),
        _optimize(opt),
        _type_sig(type_signature),
        simplify_candidate(si_ca),
        simplify_knob_building(si_kb),
        _cscorer(sc),
        _params(pa)
    {}

    ~deme_expander()
    {
        if (_rep) delete _rep;
        if (_deme) delete _deme;
    }

    /**
     * Create the deme
     *
     * @return return true if it creates deme successfully,otherwise false.
     */
    // bool create_deme(bscored_combo_tree_set::const_iterator exemplar)
    bool create_deme(const combo_tree& exemplar);

    /**
     * Do some optimization according to the scoring function.
     *
     * @param max_evals the max evals
     *
     * @return return the number of evaluations actually performed,
     */
    int optimize_deme(int max_evals);

    void free_deme();

    // Structures related to the current deme
    representation* _rep; // representation of the current deme
    deme_t* _deme; // current deme
    optimizer_base &_optimize;

protected:
    const combo::type_tree& _type_sig;    // type signature of the exemplar
    const reduct::rule& simplify_candidate; // to simplify candidates
    const reduct::rule& simplify_knob_building; // during knob building
    const cscore_base& _cscorer; // composite score
    metapop_parameters _params;

    // exemplar of the current deme; a copy, not a reference.
    combo_tree _exemplar;
};

/**
 * The metapopulation will store the expressions (as scored trees)
 * that were encountered during the learning process.  Only the
 * highest-scoring trees are typically kept.
 *
 * The metapopulation is updated in iterations. In each iteration, one
 * of its elements is selected as an exemplar. The exemplar is then
 * decorated with knobs and optimized, to create a new deme.  Members
 * of the deme are then folded back into the metapopulation.
 *
 * NOTE:
 *   cscore_base = scoring function (output composite scores)
 *   bscore_base = behavioral scoring function (output behaviors)
 */
struct metapopulation : bscored_combo_tree_ptr_set
{
    typedef deme_t::iterator deme_it;
    typedef deme_t::const_iterator deme_cit;

    // The goal of using unordered_set here is to have O(1) access time
    // to see if a combo tree is in the set, or not.
    typedef boost::unordered_set<combo_tree,
                                 boost::hash<combo_tree> > combo_tree_hash_set;

    // Init the metapopulation with the following set of exemplars.
    void init(const std::vector<combo_tree>& exemplars,
              const reduct::rule& simplify_candidate,
              const cscore_base& cscorer);

    /**
     *  Constuctor for the class metapopulation
     *
     * @param bases   Exemplars used to initialize the metapopulation
     * @param tt      Type signature of expression to be learned.
     *                That is, the expression must have this signature,
     *                relating the argument variables, and the output type.
     * @param si_ca   Reduct rule for reducing candidate combo trees.
     * @param si_kb   Reduct rule for reducing trees decorated with
     *                knobs.
     * @param sc      Function for scoring combo trees.
     * @param bsc     Behavior scoring function
     * @param opt     Algorithm that find best knob settings for a given
     *                exemplar decorated with knobs.
     * @param pa      Control parameters for this class.
     */
    metapopulation(const std::vector<combo_tree>& bases,
                   const type_tree& type_signature,
                   const reduct::rule& si_ca,
                   const reduct::rule& si_kb,
                   const cscore_base& sc,
                   const bscore_base& bsc,
                   optimizer_base& opt,
                   const metapop_parameters& pa = metapop_parameters()) :
        _dex(type_signature, si_ca, si_kb, sc, opt, pa),
        _bscorer(bsc), params(pa),
        _merge_count(0),
        _best_cscore(worst_composite_score),
        _cached_ddp(pa.diversity_p_norm,
                    pa.diversity_pressure,
                    pa.diversity_exponent)
    {
        init(bases, si_ca, sc);
    }

    // Like above but using a single base, and a single reduction rule.
    /// @todo use C++11 redirection
    metapopulation(const combo_tree& base,
                   const type_tree& type_signature,
                   const reduct::rule& si,
                   const cscore_base& sc, const bscore_base& bsc,
                   optimizer_base& opt,
                   const metapop_parameters& pa = metapop_parameters()) :
        _dex(type_signature, si, si, sc, opt, pa),
        _bscorer(bsc), params(pa),
        _merge_count(0),
        _best_cscore(worst_composite_score),
        _cached_ddp(pa.diversity_p_norm,
                    pa.diversity_pressure,
                    pa.diversity_exponent)
    {
        std::vector<combo_tree> bases(1, base);
        init(bases, si, sc);
    }

    ~metapopulation() {}
        
    /**
     * Return the best composite score.
     */
    const composite_score& best_composite_score() const
    {
        return _best_cscore;
    }

    /**
     * Return the best score.
     */
    score_t best_score() const
    {
        return get_score(_best_cscore);
    }

    /**
     * Return the set of candidates with the highest composite
     * scores.  These will all have the the same "best_composite_score".
     */
    const metapop_candidates& best_candidates() const
    {
        return _best_candidates;
    }

    /**
     * Return the best combo tree (shortest best candidate).
     */
    const combo_tree& best_tree() const
    {
        return _best_candidates.begin()->first;
    }

    typedef score_t dp_t;  // diversity_penalty type
    
    /**
     * Distort a diversity penalty component between 2
     * candidates. (actually not used apart from a comment of
     * aggregated_dps)
     */
    dp_t distort_dp(dp_t dp) const {
        return pow(dp, params.diversity_exponent);
    }
    /**
     * The inverse function of distort_dp normalized by the vector
     * size. Basically aggregated_dps(1/N * sum_i distort_dp(x_i)) ==
     * generalized_mean(x) where N is the size of x.
     */
    dp_t aggregated_dps(dp_t ddp_sum, unsigned N) const {
        return pow(ddp_sum / N, 1.0 / params.diversity_exponent);
    }
        
    /**
     * Compute the diversity penalty for all models of the metapopulation.
     *
     * If the diversity penalty is enabled, then punish the scores of
     * those exemplars that are too similar to the previous ones.
     * This may not make any difference for the first dozen exemplars
     * choosen, but starts getting important once the metapopulation
     * gets large, and the search bogs down.
     * 
     * XXX The implementation here results in a lot of copying of
     * behavioral scores and combo trees, and thus could hurt
     * performance by quite a bit.  To avoid this, we'd need to change
     * the use of bscored_combo_tree_set in this class. This would be
     * a fairly big task, and it's currently not clear that its worth
     * the effort, as diversity_penalty is not yet showing promising
     * results...
     */
    void set_diversity();

    void log_selected_exemplar(const_iterator exemplar_it);

    /**
     * Select the exemplar from the population. An exemplar is choosen
     * from the pool of candidates using a Boltzmann distribution
     * exp (-score / temperature).  Thus, they choosen exemplar will
     * typically be high-scoring, but not necessarily the highest-scoring.
     * This allows a range of reasonably-competitive candidates to be
     * explored, and, in practice, prooves to be much more effective
     * than a greedy algo which only selects the highest-scoring candidate.
     *
     * Current experimental evidence shows that temperatures in the
     * range of 6-12 work best for most problems, both discrete
     * (e.g. 4-parity) and continuous.
     *
     * @return the iterator of the selected exemplar, if no such
     *         exemplar exists then return end()
     */
    bscored_combo_tree_ptr_set::const_iterator select_exemplar();

    /// Given the current complexity temp, return the range of scores that
    /// are likely to be selected by the select_exemplar routine. Due to
    /// exponential decay of scores in select_exemplar(), this is fairly
    /// narrow: viz: e^30 = 1e13 ... We could probably get by with
    /// e^14 = 1.2e6
    //
    score_t useful_score_range() const
    {
        return params.complexity_temperature * 30.0 / 100.0;
    }

    /// Merge candidates in to the metapopulation.
    ///
    /// If the include-dominated flag is not set, the set of candidates
    /// might be changed during merge, with the dominated candidates
    /// removed during the merge. XXX Really?  It looks like the code
    /// does this culling *before* this method is called ...
    ///
    /// Safe to call in a multi-threaded context.
    ///
    /// @todo it would probably be more efficient to use
    /// bscored_combo_tree_ptr_set and not having to copy and
    /// reallocate candidates onces they are selected. It might be
    /// minor though in terms of performance gain.
    void merge_candidates(bscored_combo_tree_set& candidates);

    /**
     * merge deme -- convert instances to trees, and save them.
     *
     * 1) cull the poorest scoring instances.
     * 2) convert set of instances to trees
     * 3) merge trees into the metapopulation, possibly using domination
     *    as the merge criterion.
     *
     * Return true if further deme exploration should be halted.
     *
     * This is almost but not quite thread-safe.  The use of
     * _visited_exemplars is not yet protected. There may be other
     * things.
     */
    bool merge_deme(deme_t* __deme, representation* __rep, size_t evals);

    /**
     * Weed out excessively bad scores. The select_exemplar() routine
     * picks an exemplar out of the metapopulation, using an
     * exponential distribution of the score. Scores that are much
     * worse than the best scores are extremely unlikely to be
     * choosen, so discard these from the metapopulation.  Keeping the
     * metapop small brings huge benefits to the mem usage and runtime
     * performance.
     *
     * However, lets not get over-zelous; if the metapop is too small,
     * then we have the nasty situation where none of the best-scoring
     * individuals lead to a solution.  Fix the minimum metapop size
     * to, oh, say, 250.
     *
     * But if the population starts exploding, this is also bad, as it
     * chews up RAM with unlikely exemplars. Keep it in check by
     * applying more and more stringent bounds on the allowable
     * scores.  The current implementation of useful_score_range()
     * returns a value a bit on the large size, by a factor of 2 or
     * so, so its quite OK to cut back on this value.
     */
    void resize_metapop();
    
    // Return the set of candidates not present in the metapopulation.
    // This makes merging faster because it decreases the number of
    // calls of dominates.
    bscored_combo_tree_set get_new_candidates(const metapop_candidates& mcs);

    typedef pair<bscored_combo_tree_set,
                 bscored_combo_tree_set> bscored_combo_tree_set_pair;

    typedef std::vector<const bscored_combo_tree*> bscored_combo_tree_ptr_vec;
    typedef bscored_combo_tree_ptr_vec::iterator bscored_combo_tree_ptr_vec_it;
    typedef bscored_combo_tree_ptr_vec::const_iterator bscored_combo_tree_ptr_vec_cit;
    typedef pair<bscored_combo_tree_ptr_vec,
                 bscored_combo_tree_ptr_vec> bscored_combo_tree_ptr_vec_pair;

    // reciprocal of random_access_view
    static bscored_combo_tree_set
    to_set(const bscored_combo_tree_ptr_vec& bcv);

    void remove_dominated(bscored_combo_tree_set& bcs, unsigned jobs = 1);

    static bscored_combo_tree_set
    get_nondominated_iter(const bscored_combo_tree_set& bcs);

    // split in 2 of equal size
    static bscored_combo_tree_ptr_vec_pair
    inline split(const bscored_combo_tree_ptr_vec& bcv)
    {
        bscored_combo_tree_ptr_vec_cit middle = bcv.begin() + bcv.size() / 2;
        return make_pair(bscored_combo_tree_ptr_vec(bcv.begin(), middle),
                         bscored_combo_tree_ptr_vec(middle, bcv.end()));
    }

    bscored_combo_tree_ptr_vec
    get_nondominated_rec(const bscored_combo_tree_ptr_vec& bcv,
                         unsigned jobs = 1);

    // return a pair of sets of nondominated candidates between bcs1
    // and bcs2, assuming none contain dominated candidates. Contrary
    // to what the name of function says, the 2 sets do not need be
    // disjoint, however there are indeed disjoint according to the way
    // they are used in the code. The first (resp. second) element of
    // the pair corresponds to the nondominated candidates of bcs1
    // (resp. bcs2)
    bscored_combo_tree_set_pair
    get_nondominated_disjoint(const bscored_combo_tree_set& bcs1,
                              const bscored_combo_tree_set& bcs2,
                              unsigned jobs = 1);

    bscored_combo_tree_ptr_vec_pair
    get_nondominated_disjoint_rec(const bscored_combo_tree_ptr_vec& bcv1,
                                  const bscored_combo_tree_ptr_vec& bcv2,
                                  unsigned jobs = 1);

    // merge nondominated candidate to the metapopulation assuming
    // that bcs contains no dominated candidates within itself
    void merge_nondominated(bscored_combo_tree_set& bcs, unsigned jobs = 1);

    // Iterative version of merge_nondominated
    void merge_nondominated_iter(bscored_combo_tree_set& bcs);

    /**
     * return true if x dominates y
     *        false if y dominates x
     *        indeterminate otherwise
     */
    static inline tribool dominates(const behavioral_score& x,
                                    const behavioral_score& y)
    {
        // everything dominates an empty vector
        if (x.empty()) {
            if (y.empty())
                return indeterminate;
            return false;
        } else if (y.empty()) {
            return true;
        }

        tribool res = indeterminate;
        for (behavioral_score::const_iterator xit = x.begin(), yit = y.begin();
             xit != x.end();++xit, ++yit)
        {
            if (*xit > *yit) {
                if (!res)
                    return indeterminate;
                else
                    res = true;
            } else if (*yit > *xit) {
                if (res)
                    return indeterminate;
                else
                    res = false;
            }
        }
        return res;
    }

    /// Update the record of the best score seen, and the associated tree.
    /// Safe to call in a multi-threaded context.
    void update_best_candidates(const bscored_combo_tree_set& candidates);

    // log the best candidates
    void log_best_candidates() const;

    /**
     * stream out the metapopulation in decreasing order of their
     * score along with their scores (optionally complexity and
     * bscore).  If n is negative, then stream them all out.  Note
     * that the default sort order for the metapop is a penalized
     * scores and the smallest complexities, so that the best-ranked
     * candidates are not necessarily those with the best raw score.
     */
    template<typename Out, typename In>
    Out& ostream(Out& out, In from, In to, long n = -1,
                 bool output_score = true,
                 bool output_penalty = false,
                 bool output_bscore = false,
                 bool output_only_bests = false,
                 bool output_python = false)
    {
        if (!output_only_bests) {
            for (; from != to && n != 0; ++from, n--) {
                ostream_bscored_combo_tree(out, *from, output_score,
                                           output_penalty, output_bscore,
                                           output_python);
            }
            return out;
        }

        // Else, search for the top score...
        score_t best_score = very_worst_score;

        for (In f = from; f != to; ++f) {
            const bscored_combo_tree& bt = *f;
            score_t sc = get_score(bt);
            if (best_score < sc) best_score = sc;
        }

        // And print only the top scorers.
        // The problem here is that the highest scorers are not
        // necessarily ranked highest, as the ranking is a linear combo
        // of both score and complexity.
        for (In f = from; f != to && n != 0; ++f, n--) {
            const bscored_combo_tree& bt = *f;
            if (best_score <= get_score(bt)) {
                ostream_bscored_combo_tree(out, bt, output_score,
                                           output_penalty, output_bscore,
                                           output_python);
            }
        }
        return out;
    }

    // Like above, but assumes that from = begin() and to = end().
    template<typename Out>
    Out& ostream(Out& out, long n = -1,
                 bool output_score = true,
                 bool output_penalty = false,
                 bool output_bscore = false,
                 bool output_only_bests = false,
                 bool output_python = false)
    {
        return ostream(out, begin(), end(),
                       n, output_score, output_penalty,
                       output_bscore, output_only_bests, output_python);
    }

    ///  hmmm, apparently it's ambiguous with the one below, strange
    // // Like above but assumes that from = c.begin() and to = c.end()
    // template<typename Out, typename C>
    // Out& ostream(Out& out, const C& c, long n = -1,
    //              bool output_score = true,
    //              bool output_complexity = false,
    //              bool output_bscore = false,
    //              bool output_dominated = false)
    // {
    //     return ostream(out, c.begin(), c.end(),
    //                    n, output_score, output_complexity,
    //                    output_bscore, output_dominated);
    // }

    // Like above, but using std::cout.
    void print(long n = -1,
               bool output_score = true,
               bool output_penalty = false,
               bool output_bscore = false,
               bool output_only_bests = false);

    deme_expander _dex;

protected:
    static const unsigned min_pool_size = 250;
    
    const bscore_base& _bscorer; // behavioral score
    metapop_parameters params;

    size_t _merge_count;

    // The best score ever found during search.
    composite_score _best_cscore;

    // Trees with composite score equal to _best_cscore.
    metapop_candidates _best_candidates;

    // contains the exemplars of demes that have been searched so far
    combo_tree_hash_set _visited_exemplars;

    // lock to enable thread-safe deme merging.
    std::mutex _merge_mutex;

    /**
     * Cache for distorted diversity penalty. Maps a
     * std::set<bscored_combo_tree*> (only 2 elements to represent an
     * unordered pair) to an distorted diversity penalty. We don't use
     * {lru,prr}_cache because
     *
     * 1) we don't need a limit on the cache.
     *
     * 2) however we need a to remove the pairs containing deleted pointers
     */
    struct cached_ddp {
        // ctor
        cached_ddp(dp_t lp_norm, dp_t diversity_pressure, dp_t diversity_exponent)
            : p(lp_norm), dpre(diversity_pressure), dexp(diversity_exponent),
              misses(0), hits(0) {}

        // We use a std::set instead of a std::pair, little
        // optimization to deal with the symmetry of the distance
        typedef std::set<const bscored_combo_tree*> ptr_pair; 
        dp_t operator()(const bscored_combo_tree* cl, const bscored_combo_tree* cr)
        {
            ptr_pair cts = {cl, cr};
            // hit
            {
                shared_lock lock(mutex);
                auto it = cache.find(cts);
                if (it != cache.end()) {
                    ++hits;
                    return it->second;
                }
            }
            // miss
            dp_t dst = lp_distance(get_bscore(*cl), get_bscore(*cr), p),
                dp = dpre / (1.0 + dst),
                ddp = dexp > 0.0 ? pow(dp, dexp) : dp /* no distortion */;

            // // debug
            // logger().fine("dst = %f, dp = %f, ddp = %f", dst, dp, ddp);
            // // ~debug
            
            ++misses;
            {
                unique_lock lock(mutex);
                return cache[cts] = ddp;
            }
        }

        /**
         * Remove all keys containing any element of ptr_seq
         */
        void erase_ptr_seq(std::vector<bscored_combo_tree*> ptr_seq) {
            for (Cache::iterator it = cache.begin(); it != cache.end();) {
                if (!is_disjoint(ptr_seq, it->first))
                    it = cache.erase(it);
                else
                    ++it;
            }
        }

        // cache
        typedef boost::shared_mutex cache_mutex;
        typedef boost::shared_lock<cache_mutex> shared_lock;
        typedef boost::unique_lock<cache_mutex> unique_lock;
        typedef boost::unordered_map<ptr_pair, dp_t, boost::hash<ptr_pair>> Cache;
        cache_mutex mutex;

        dp_t p, dpre, dexp;
        std::atomic<unsigned> misses, hits;
        Cache cache;
    };

    cached_ddp _cached_ddp;
};

} // ~namespace moses
} // ~namespace opencog

#endif // _OPENCOG_METAPOPULATION_H
