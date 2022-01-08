#include <cstdio>
#include <unordered_set>
#include <array>
#include "Map.h"
// for random generator
#include "fph/dynamic_fph_table.h"


#ifndef FPH_HAVE_BUILTIN
#ifdef __has_builtin
#define FPH_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define FPH_HAVE_BUILTIN(x) 0
#endif
#endif

#ifndef FPH_HAS_BUILTIN_OR_GCC_CLANG
#if defined(__GNUC__) || defined(__clang__)
#   define FPH_HAS_BUILTIN_OR_GCC_CLANG(x) 1
#else
#   define FPH_HAS_BUILTIN_OR_GCC_CLANG(x) FPH_HAVE_BUILTIN(x)
#endif
#endif

#ifndef FPH_LIKELY
#ifndef FPH_LIKELY
#   if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_expect)
#       define FPH_LIKELY(x) (__builtin_expect((x), 1))
#   else
#       define FPH_LIKELY(x) (x)
#   endif
#endif
#endif

#ifndef FPH_UNLIKELY
#ifndef FPH_UNLIKELY
#   if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_expect)
#       define FPH_UNLIKELY(x) (__builtin_expect((x), 0))
#   else
#       define FPH_UNLIKELY(x) (x)
#   endif
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
#define BENCH_OS_WIN
#include <windows.h>  // NOLINT
#endif

// from absl
// Prevents the compiler from eliding the computations that led to "output".
template <class T>
inline void PreventElision(T&& output) {
#ifndef BENCH_OS_WIN
    // Works by indicating to the compiler that "output" is being read and
    // modified. The +r constraint avoids unnecessary writes to memory, but only
    // works for built-in types (typically FuncOutput).
    asm volatile("" :: "g" (output));
#else
#include <atomic>
    // MSVC does not support inline assembly anymore (and never supported GCC's
  // RTL constraints). Self-assignment with #pragma optimize("off") might be
  // expected to prevent elision, but it does not with MSVC 2015. Type-punning
  // with volatile pointers generates inefficient code on MSVC 2017.
  static std::atomic<T> dummy(T{});
  dummy.store(output, std::memory_order_relaxed);
#endif
}


template <typename>
struct is_pair : std::false_type
{ };

template <typename T, typename U>
struct is_pair<std::pair<T, U>> : std::true_type
{ };

template<class T, typename = void>
struct SimpleGetKey {
    const T& operator()(const T& x) const {
        return x;
    }

    T& operator()(T& x) {
        return x;
    }

    T operator()(T&& x) {
        return std::move(x);
    }

};

template<class T>
struct SimpleGetKey<T, typename std::enable_if<is_pair<T>::value, void>::type> {

    using key_type = typename T::first_type;


    const key_type& operator()(const T& x) const {
        return x.first;
    }

    key_type& operator()(T& x) {
        return x.first;
    }

    key_type operator()(T&& x) {
        return std::move(x.first);
    }
};

template<typename T, typename = void>
struct MutableValue {
    using type = T;
};

template<typename T>
struct MutableValue<T, typename std::enable_if<is_pair<T>::value>::type> {
    using type = std::pair<typename std::remove_const<typename T::first_type>::type, typename T::second_type>;
};

template<class T>
std::string ToString(const T& t) {
    return std::to_string(t);
}


std::string ToString(const std::string& t) {
    return t;
}

std::string ToString(const char* src) {
    return src;
}

template<class Table, class PairVec, class GetKey = SimpleGetKey<typename PairVec::value_type>>
void ConstructTable(Table &table, const PairVec &pair_vec, bool do_reserve = true, bool do_rehash = false) {
    table.clear();
    if (do_reserve) {
        table.reserve(pair_vec.size());
    }
    for (size_t i = 0; i < pair_vec.size(); ++i) {
        const auto &pair = pair_vec[i];
        table.insert(pair);
    }
    if (do_rehash) {
        if (table.load_factor() < 0.5) {
            table.max_load_factor(0.9);
            table.rehash(table.size());
        }
    }


}

template<bool do_reserve = true, bool verbose = true, class Table, class PairVec,
        class GetKey = SimpleGetKey<typename PairVec::value_type>>
uint64_t TestTableConstruct(Table &table, const PairVec &pair_vec) {
    table.clear();
    auto begin_time = std::chrono::high_resolution_clock::now();
    ConstructTable(table, pair_vec, do_reserve, false);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto pass_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
    if constexpr (verbose) {
        fprintf(stderr, "%s construct use time %.6f seconds",
                MAP_NAME, (double)pass_ns / (1e+9));
    }
    return pass_ns;
}

enum LookupExpectation {
    KEY_IN = 0,
    KEY_NOT_IN,
    KEY_MAY_IN,
};

template<LookupExpectation LOOKUP_EXP, bool verbose = true, class Table, class PairVec,
        class GetKey = SimpleGetKey<typename PairVec::value_type> >
std::tuple<uint64_t, uint64_t> TestTableLookUp(Table &table, size_t lookup_time, const PairVec &input_vec,
                                               const PairVec &lookup_vec, size_t seed,
                                               bool set_load_factor = false) {

    size_t look_up_index = 0;
    size_t key_num = input_vec.size();
    uint64_t useless_sum = 0;
    if (input_vec.empty()) {
        return {0, 0};
    }
    std::mt19937_64 random_engine(seed);
    std::uniform_int_distribution<size_t> random_dis;
    auto pair_vec = lookup_vec;
    auto start_construct_t = std::chrono::high_resolution_clock::now();
    try {
        ConstructTable<Table, PairVec, GetKey>(table, input_vec, true, set_load_factor);
    } catch(std::exception& e) {
        fprintf(stderr, "Catch exception in Construct when TestTableLookUp, set_max_load_factor: %d, msg:%s\n",
                set_load_factor, e.what());
        return {0, 0};
    }
    auto end_construct_t = std::chrono::high_resolution_clock::now();
    auto construct_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_construct_t - start_construct_t).count();
    std::shuffle(pair_vec.begin(), pair_vec.end(), random_engine);

    auto look_up_t0 = std::chrono::high_resolution_clock::now();
    for (size_t t = 0; t < lookup_time; ++t) {
        ++look_up_index;
        if FPH_UNLIKELY(look_up_index >= key_num) {
            look_up_index -= key_num;
        }
        if constexpr (LOOKUP_EXP == KEY_IN) {
            PreventElision(table.find(GetKey{}(pair_vec[look_up_index])));
//            auto find_it = table.find(GetKey{}(pair_vec[look_up_index]));
//            useless_sum += *reinterpret_cast<uint8_t*>(std::addressof(find_it->second));
        }
        else if constexpr (LOOKUP_EXP == KEY_NOT_IN) {
            auto find_it = table.find(GetKey{}(pair_vec[look_up_index]));
            if FPH_UNLIKELY(find_it != table.end()) {
                fprintf(stderr, "Find key %s in table %s",
                               ToString(GetKey{}(pair_vec[look_up_index])).c_str(),
                               MAP_NAME);
                return {0, 0};
            }
        }
        else {
            PreventElision(table.find(GetKey{}(pair_vec[look_up_index])));
//            auto find_it = table.find(GetKey{}(pair_vec[look_up_index]));
//            if (find_it != table.end()) {
//                useless_sum += find_it->second;
//            }
        }

    }
    auto look_up_t1 = std::chrono::high_resolution_clock::now();
    auto look_up_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(look_up_t1 - look_up_t0).count();
    if constexpr (verbose) {
        fprintf(stderr, "%s look up use %.3f ns per call, use_less sum: %lu",
                       MAP_NAME, (double)look_up_ns * 1.0 / (double)lookup_time,
                       useless_sum);
    }
    return {look_up_ns, construct_ns};
}

template<class Table, class PairVec,
        class GetKey = SimpleGetKey<typename PairVec::value_type> >
std::tuple<uint64_t, uint64_t> TestTableIterate(Table &table, size_t iterate_time, const PairVec &input_vec) {
    try {
        ConstructTable<Table, PairVec, GetKey>(table, input_vec);
    }   catch(std::exception &e) {
        fprintf(stderr, "Catch exception when TestTableIterate, hash:%s, map:%s\n%s\n",
                HASH_NAME, MAP_NAME, e.what());
        return {0, 0};
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t useless_sum = 0;
    for (size_t t = 0; t < iterate_time; ++t) {
        for (auto it = table.begin(); it != table.end(); ++it) {
            // If second is not int
            PreventElision(std::addressof(it->second));
//            useless_sum += *reinterpret_cast<uint8_t*>(std::addressof(it->second));
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t pass_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    return {pass_ns, useless_sum};

}

template<class T, class RNG>
void ConstructRngPtr(std::unique_ptr<RNG>& key_rng_ptr, size_t seed, size_t max_possible_value) {
    if constexpr(std::is_integral_v<T>) {
        key_rng_ptr = std::make_unique<RNG>(seed, max_possible_value);
    }
    else {
        key_rng_ptr = std::make_unique<RNG>(seed);
    }
}

//using StatsTuple = std::tuple<double, double, double, double, double, double, double, double, double>;
using StatsTuple = std::array<double, 10>;

template<class ValueRandomGen, class Table, class value_type,
        class GetKey = SimpleGetKey<value_type>>
StatsTuple TestTablePerformance(size_t element_num, size_t construct_time, size_t lookup_time,
                          size_t seed = 0) {
    std::mt19937_64 random_engine(seed);
    std::uniform_int_distribution<size_t> size_gen;
//    using value_type = typename Table::value_type;
    using mutable_value_type = typename MutableValue<value_type>::type;

    using key_type = typename Table::key_type;
    using mapped_type = typename Table::mapped_type;

    using KeyRNG = typename ValueRandomGen::KeyRNGType;
    using ValueRNG = typename ValueRandomGen::ValueRNGType;

//    std::unique_ptr<ValueRandomGen> value_gen_ptr;
    std::unique_ptr<KeyRNG> key_rng_ptr;
    std::unique_ptr<ValueRNG> value_rng_ptr;
    ConstructRngPtr<key_type, KeyRNG>(key_rng_ptr, seed, element_num);
    ConstructRngPtr<mapped_type, ValueRNG>(value_rng_ptr, seed, element_num);
    ValueRandomGen value_gen{seed, std::move(*key_rng_ptr), std::move(*value_rng_ptr)};
    value_gen.seed(seed);

    std::unordered_set<key_type> key_set;
    key_set.reserve(element_num);

    std::vector<mutable_value_type> src_vec;
    src_vec.reserve(element_num);
    for (size_t i = 0; i < element_num; ++i) {
        auto temp_pair = value_gen();
        while (key_set.find(GetKey{}(temp_pair)) != key_set.end()) {
            temp_pair = value_gen();
        }
        src_vec.push_back(temp_pair);
        key_set.insert(GetKey{}(temp_pair));
    }



    size_t construct_seed = size_gen(random_engine);
    size_t total_reserve_construct_ns = 0;

    try {
        for (size_t t = 0; t < construct_time; ++t) {
            Table table;
            uint64_t temp_construct_ns = TestTableConstruct<true, false>
                    (table, src_vec);
            total_reserve_construct_ns += temp_construct_ns;
//        print_load_factor = table.load_factor();
        }
    } catch(std::exception &e) {
        fprintf(stderr, "Catch exception when TestTableConstruct with reserve, element_num: %lu\n%s\n",
                element_num, e.what());
        total_reserve_construct_ns = 0;
    }

    size_t total_no_reserve_construct_ns = 0;
    try {
        for (size_t t = 0; t < construct_time; ++t) {
            Table table;
            uint64_t temp_construct_ns = TestTableConstruct<false, false>
                    (table, src_vec);
            total_no_reserve_construct_ns += temp_construct_ns;
        }
    }   catch(std::exception &e) {
        fprintf(stderr, "Catch exception when TestTableConstruct without reserve, element_num: %lu\n%s\n",
                element_num, e.what());
        total_no_reserve_construct_ns = 0;
    }


    float no_rehash_load_factor = 0;
    uint64_t in_no_rehash_lookup_ns = 0;
    {
        Table table;
        std::tie(in_no_rehash_lookup_ns, std::ignore) = TestTableLookUp<KEY_IN, false, Table,
                std::vector<mutable_value_type>, GetKey>(table, lookup_time, src_vec,
                                                         src_vec, construct_seed, false);
        no_rehash_load_factor = table.load_factor();
    }

    // generate a list of key not in the key set
    std::vector<mutable_value_type> lookup_vec;
    lookup_vec.reserve(src_vec.size());
    std::unique_ptr<KeyRNG> miss_key_rng_ptr;
    ConstructRngPtr<key_type, KeyRNG>(miss_key_rng_ptr, random_engine(), std::numeric_limits<size_t>::max());
    ConstructRngPtr<mapped_type, ValueRNG>(value_rng_ptr, random_engine(), std::numeric_limits<size_t>::max());
    ValueRandomGen miss_value_gen{random_engine(), std::move(*miss_key_rng_ptr), std::move(*value_rng_ptr)};
    for (size_t i = 0; i < src_vec.size(); ++i) {
        auto temp_pair = miss_value_gen();

        while (key_set.find(GetKey{}(temp_pair)) != key_set.end()) {
            temp_pair = miss_value_gen();
        }
        lookup_vec.push_back(temp_pair);
    }

    uint64_t out_no_rehash_lookup_ns = 0;
    {
        Table table;
        std::tie(out_no_rehash_lookup_ns, std::ignore) = TestTableLookUp<KEY_NOT_IN, false>(table, lookup_time, src_vec,
                                                                                              lookup_vec, construct_seed, false);
    }

    float with_rehash_load_factor = 0;
    uint64_t in_with_rehash_lookup_ns = 0, in_with_rehash_construct_ns = 0;
    {
        Table table;
        std::tie(in_with_rehash_lookup_ns, in_with_rehash_construct_ns) = TestTableLookUp<KEY_IN, false, Table,
                std::vector<mutable_value_type>, GetKey>(table, lookup_time, src_vec,
                                                         src_vec, construct_seed, true);
        with_rehash_load_factor = table.load_factor();
    }

    uint64_t out_with_rehash_lookup_ns = 0, out_with_rehash_construct_ns = 0;
    {
        Table table;
        std::tie(out_with_rehash_lookup_ns, out_with_rehash_construct_ns) = TestTableLookUp<KEY_NOT_IN, false>(table, lookup_time, src_vec,
                                                                                            lookup_vec, construct_seed, true);
    }



    uint64_t iterate_ns = 0, it_useless_sum = 0;
    uint64_t iterate_time = (lookup_time + element_num - 1) / element_num / 4UL;
    {

        Table table;
        std::tie(iterate_ns, it_useless_sum) = TestTableIterate<Table,
                std::vector<mutable_value_type>, GetKey>(
                table, lookup_time / element_num, src_vec);
    }

    double avg_construct_time_with_reserve_ns = (double)total_reserve_construct_ns / (double)construct_time / (double)element_num;
    double avg_construct_time_without_reserve_ns = (double)total_no_reserve_construct_ns / (double)construct_time / (double)element_num;
    double avg_hit_without_rehash_lookup_ns = (double)in_no_rehash_lookup_ns / (double)lookup_time;
    double avg_miss_without_rehash_lookup_ns = (double)out_no_rehash_lookup_ns / (double)lookup_time;
    double avg_hit_with_rehash_lookup_ns = (double)in_with_rehash_lookup_ns  / (double)lookup_time;

    double avg_miss_with_rehash_lookup_ns = (double)out_with_rehash_lookup_ns / (double)lookup_time;
    double avg_iterate_ns = (double)iterate_ns / double(iterate_time * element_num);
    double avg_final_rehash_construct_ns = (double)(in_with_rehash_construct_ns + out_with_rehash_construct_ns) / (double)(element_num * 2ULL);

    fprintf(stderr, "%s %lu elements, sizeof(value_type)=%lu, construct with reserve avg use %.6f s,"
                    "construct without reserve avg use %.6f s, "
                    "no rehash normal construct got load_factor: %.6f, "
                    "with rehash after construct got load_factor: %.6f, "
                    "look up key hit without rehash use %.3f ns per key,"
                    "look up miss without rehash use %.3f ns per key, "
                    "look up key hit with rehash use %.3f ns per key,"
                    "look up miss with rehash use %.3f ns per key, "
                    "iterate use %.3f ns per value, "
                    "with_final_rehash_construct_time: %.3f s\n",
                MAP_NAME, element_num, sizeof(value_type),
                (double)total_reserve_construct_ns / (1e+9) / (double)construct_time,
                (double)total_no_reserve_construct_ns / (1e+9) / (double)construct_time,
                no_rehash_load_factor, with_rehash_load_factor,
                (double)in_no_rehash_lookup_ns / (double)lookup_time,
                (double)out_no_rehash_lookup_ns / (double)lookup_time,
                (double)in_with_rehash_lookup_ns  / (double)lookup_time,
                (double)out_with_rehash_lookup_ns / (double)lookup_time,
                (double)iterate_ns / double(iterate_time * element_num),
                double(in_with_rehash_construct_ns + out_with_rehash_lookup_ns) / (1e+9) / 2.0
            );

    return {avg_construct_time_with_reserve_ns, avg_construct_time_without_reserve_ns,
            no_rehash_load_factor, with_rehash_load_factor,
            avg_hit_without_rehash_lookup_ns, avg_miss_without_rehash_lookup_ns,
            avg_hit_with_rehash_lookup_ns, avg_miss_with_rehash_lookup_ns,
            avg_iterate_ns, avg_final_rehash_construct_ns
            };


}

template<class T1, class T2, class T1RNG = fph::dynamic::RandomGenerator<T1>, class T2RNG = fph::dynamic::RandomGenerator<T2>>
class RandomPairGen {
public:
//    RandomPairGen(size_t seed): init_seed(seed), t1_gen(init_seed), t2_gen(init_seed) {}
    RandomPairGen(size_t first_seed, T1RNG&& t1_rng, T2RNG&& t2_rng):
            init_seed(first_seed), t1_gen(std::move(t1_rng)), t2_gen(std::move(t2_rng)) {
        seed(first_seed);
    }

    std::pair<T1,T2> operator()() {
        return {t1_gen(), t2_gen()};
    }

    template<class SeedType>
    void seed(SeedType seed) {
        t1_gen.seed(seed);
        t2_gen.seed(seed);
    }

    size_t init_seed;

    using KeyRNGType = T1RNG;
    using ValueRNGType = T2RNG;

protected:
    T1RNG t1_gen;
    T2RNG t2_gen;
};

enum KeyBitsPattern{
    UNIFORM = 0,
    MASK_LOW_BITS ,
    MASK_HIGH_BITS,
};


template<KeyBitsPattern key_pattern>
class MaskedUint64RNG {
public:

//    MaskedUint64RNG(): init_seed(std::random_device{}()), random_engine(init_seed) {}

    MaskedUint64RNG(size_t seed, size_t max_element_size): init_seed(seed), random_engine(seed),
                        mask(0){
        size_t mask_len = fph::dynamic::detail::RoundUpLog2(max_element_size);
        if constexpr(key_pattern == MASK_LOW_BITS) {
            size_t low_mask = fph::dynamic::detail::GenBitMask<uint64_t>(std::numeric_limits<size_t>::digits - mask_len);
            mask = ~low_mask;
        }
        else if constexpr(key_pattern == MASK_HIGH_BITS) {
            mask = fph::dynamic::detail::GenBitMask<uint64_t>(mask_len);
        }

    }

    MaskedUint64RNG(const MaskedUint64RNG& other): init_seed(other.init_seed),
                                                   random_engine(other.random_engine),
                                                   random_gen(other.random_gen),
                                                   mask(other.mask) {}

    MaskedUint64RNG(MaskedUint64RNG&& other) noexcept:
            init_seed(std::exchange(other.init_seed, 0)),
            random_engine(std::move(other.random_engine)),
            random_gen(std::move(other.random_gen)),
            mask(std::exchange(other.mask, 0)){}

    MaskedUint64RNG& operator=(MaskedUint64RNG&& other) noexcept {
        this->init_seed = std::exchange(other.init_seed, 0);
        this->random_engine = std::move(other.random_engine);
        this->random_gen = std::move(other.random_gen);
        this->mask = std::exchange(other.mask, 0);
        return *this;
    }

    uint64_t operator()() {
        auto ret = random_gen(random_engine);

        if constexpr(key_pattern == MASK_HIGH_BITS || key_pattern == MASK_LOW_BITS) {
            ret &= mask;
        }
        return ret;
    }

    void seed(uint64_t seed = 0) {
        init_seed = seed;
        random_engine.seed(seed);
    }

    size_t init_seed;

protected:
    std::mt19937_64 random_engine;
    std::uniform_int_distribution<uint64_t> random_gen;
    uint64_t mask;

};

template<size_t max_len>
class StringRNG {
public:

//    StringRNG(): init_seed(std::random_device{}()), random_engine(init_seed) {}
    StringRNG(const StringRNG& other): init_seed(other.init_seed),
                                                   random_engine(other.random_engine) {}

    explicit StringRNG(size_t seed): init_seed(seed), random_engine(seed) {}

    StringRNG(StringRNG&& other) noexcept:
            init_seed(std::exchange(other.init_seed, 0)),
            random_engine(std::move(other.random_engine)){}

    StringRNG& operator=(StringRNG&& other) noexcept {
        this->init_seed = std::exchange(other.init_seed, 0);
        this->random_engine = std::move(other.random_engine);
        return *this;
    }

    std::string operator()(size_t length) {
        static constexpr char alphanum[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        std::string ret(length, 0);
        for (size_t i = 0; i < length; ++i) {
            ret[i] = alphanum[random_gen(random_engine) % (sizeof(alphanum) - 1)];
        }
        return ret;
    }

    /**
     * random generate a string consists of alpha num with random length from [1, 10]
     * @return
     */
    std::string operator()() {
        // TODO: change default size to bigger number if needed
        size_t random_len = 1UL + random_gen(random_engine) % max_len;
        return (*this)(random_len);
    }

    void seed(uint64_t seed = 0) {
        init_seed = seed;
        random_engine.seed(seed);
    }

    size_t init_seed;

protected:
    std::minstd_rand random_engine;
    std::uniform_int_distribution<uint32_t> random_gen;
};

template<size_t size>
struct FixSizeStruct {
    constexpr FixSizeStruct()noexcept: data{0} {}
    char data[size];

    friend bool operator==(const FixSizeStruct<size>&a, const FixSizeStruct<size>&b) {
        return memcmp(a.data, b.data, size) == 0;
    }
};

template<size_t size>
class FixSizeStructRNG {
public:
    FixSizeStructRNG(): init_seed(std::random_device{}()), uint_gen(init_seed) {};
    FixSizeStructRNG(size_t seed): init_seed(seed), uint_gen(seed) {}

    FixSizeStruct<size> operator()() {
        FixSizeStruct<size> ret{};
        *reinterpret_cast<uint8_t*>(&ret) = (uint8_t)uint_gen();
        return ret;
//        return FixSizeStruct(string_gen());
    }

    void seed(size_t seed) {
        init_seed = seed;
        uint_gen.seed(seed);
    }

    size_t init_seed;

protected:
    fph::dynamic::RandomGenerator<uint64_t> uint_gen;
//    fph::dynamic::RandomGenerator<std::string> string_gen;
};

constexpr char PathSeparator() {
#ifdef BENCH_OS_WIN
    return '\\';
#else
    return '/';
#endif
}

template<class KeyType, class ValueType, class KeyRandomGen, class ValueRandomGen>
auto TestOnePairType( size_t seed, const std::vector<size_t>& key_size_array) {
//    using KeyRandomGen = fph::dynamic::RandomGenerator<KeyType>;

//    using ValueRandomGen = fph::dynamic::RandomGenerator<ValueType>;

    using RandomGenerator = RandomPairGen<KeyType, ValueType, KeyRandomGen , ValueRandomGen>;

    std::mt19937_64 uint64_rng{seed};

    using PairType = std::pair<KeyType, ValueType>;
//    constexpr size_t KEY_NUM = 84100ULL;
    //2UL, 4UL, 8UL,

    std::vector<StatsTuple> result_vec;
    for (auto key_num: key_size_array) {
        size_t LOOKUP_TIME = 200000000ULL;
        size_t CONSTRUCT_TIME = 2;
        if (key_num < 100000UL) {
            CONSTRUCT_TIME = 6;
        }
        else if (key_num < 1000000UL) {
            CONSTRUCT_TIME = 3;
        }
        else if (key_num < 10000000UL) {
            CONSTRUCT_TIME = 2;
        }

        using TestPerformanceDyFphMap = Map<KeyType, ValueType>;

        static_assert(is_pair<typename TestPerformanceDyFphMap::value_type>::value);



        StatsTuple stats_tuple = TestTablePerformance<RandomGenerator, TestPerformanceDyFphMap, PairType>(
                key_num, CONSTRUCT_TIME, LOOKUP_TIME, uint64_rng());
        result_vec.push_back(stats_tuple);

    }
    return result_vec;

//    constexpr size_t KEY_NUM = 100000000ULL;



}

void TestRNG() {
    using MaskHighBitsUint64RNG = MaskedUint64RNG<MASK_HIGH_BITS>;
    using MaskLowBitsUint64RNG = MaskedUint64RNG<MASK_LOW_BITS>;
//    using UniformUint64RNG = MaskedUint64RNG<UNIFORM>;

    fprintf(stderr, "Mask low bits 16 bit\n");
    MaskLowBitsUint64RNG low_rng_4_bit_1(0, (1UL << 16) - 4);
    for (int i = 0; i < 32; ++i) {
        fprintf(stderr, "%lx, ", low_rng_4_bit_1());
    }
    fprintf(stderr, "\n");

    MaskHighBitsUint64RNG high_rng_8bit_1(0, 256);
    fprintf(stderr, "Mask High bits 8 bit\n");
    for (int i = 0; i < 32; ++i) {
        fprintf(stderr, "%lx, ", high_rng_8bit_1());
    }
    fprintf(stderr, "\n");


}

void ExportToCsv(FILE* export_fp, const std::vector<size_t>& element_num_vec, const std::vector<StatsTuple>& result_vec) {
    const char* csv_header = "element_num,avg_construct_time_with_reserve_ns,avg_construct_time_without_reserve_ns,"
                             "no_rehash_load_factor,with_rehash_load_factor,"
                             "avg_hit_without_rehash_lookup_ns,avg_miss_without_rehash_lookup_ns,"
                             "avg_hit_with_rehash_lookup_ns,avg_miss_with_rehash_lookup_ns,"
                             "avg_iterate_ns,avg_with_final_rehash_construct_ns";
    fprintf(export_fp, "%s\n", csv_header);
    if (element_num_vec.size() != result_vec.size()) {
        fprintf(stderr, "Error, element num vec size != result vec size");
        std::abort();
    }
    for (size_t k = 0; k < element_num_vec.size(); ++k) {
        size_t element_num = element_num_vec[k];
        fprintf(export_fp, "%lu,", element_num);
        const auto& stats_tuple = result_vec[k];
        for (size_t i = 0; i < std::tuple_size_v<StatsTuple>; ++i) {
            fprintf(export_fp, "%f", stats_tuple[i]);
            if (i + 1UL != std::tuple_size_v<StatsTuple>) {
                fprintf(export_fp, ",");
            }
            else fprintf(export_fp, "\n");
        }
    }
    fclose(export_fp);
}

void BenchTest(size_t seed, const char* data_dir) {
//    TestRNG();
    using MaskHighBitsUint64RNG = MaskedUint64RNG<MASK_HIGH_BITS>;
    using MaskLowBitsUint64RNG = MaskedUint64RNG<MASK_LOW_BITS>;
    using UniformUint64RNG = MaskedUint64RNG<UNIFORM>;


    std::string map_name = std::string(MAP_NAME);
    std::string hash_name = std::string(HASH_NAME);
    std::string data_dir_path = std::string(data_dir) + PathSeparator();


    std::vector<size_t> key_size_array = {15UL, 16UL, 31UL, 32UL, 33UL, 50UL, 64UL,
                                          80UL, 110UL, 128UL, 240UL, 400UL, 500UL, 512UL, 670UL, 800UL, 1023UL,
                                          1024UL, 1500UL, 2048UL, 3000UL, 4096UL, 6000UL, 8192UL, 12000UL,
                                          16384UL, 25000UL, 32768UL, 60000UL,
                                          100000UL, 200000UL, 400000UL, 800000UL, 1600000UL, 3200000UL, 10000000UL};
//    constexpr size_t csv_head_num = StatsTuple::s

    fprintf(stderr, "\n------ Begin to test hash %s with map %s ---\n\n", HASH_NAME, MAP_NAME);
//    if constexpr (!std::is_same_v<Hash<uint64_t>, fph::MixSeedHash<uint64_t>>)
    {
        fprintf(stderr, "Test Low bits masked uint64 key\n\n");
        std::string data_file_name = map_name + "__" + hash_name + "__" + "mask_low_bits_uint64_t" + "__" + "uint64_t" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<uint64_t, uint64_t, MaskLowBitsUint64RNG, UniformUint64RNG>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

    {
        fprintf(stderr, "\nTest High bits masked uint64 key\n\n");
        std::string data_file_name = map_name + "__" + hash_name + "__" + "mask_high_bits_uint64_t" + "__" + "uint64_t" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<uint64_t, uint64_t, MaskHighBitsUint64RNG, UniformUint64RNG>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

    {
        fprintf(stderr, "\nTest Uniformly distributed uint64 key\n\n");
        std::string data_file_name = map_name + "__" + hash_name + "__" + "uniform_uint64_t" + "__" + "uint64_t" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<uint64_t, uint64_t, UniformUint64RNG, UniformUint64RNG>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

    {
        fprintf(stderr, "\nTest Uniformly distributed uint64 key and 56 bytes payload\n\n");
        std::string data_file_name = map_name + "__" + hash_name + "__" + "uniform_uint64_t" + "__" + "56bytes_payload" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<uint64_t, FixSizeStruct<56>, UniformUint64RNG, FixSizeStructRNG<56>>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

    {
        fprintf(stderr, "\nTest Small String with max length 13\n\n");
        using SmallStringRNG = StringRNG<13>;
        std::string data_file_name = map_name + "__" + hash_name + "__" + "small_string_13" + "__" + "uint64_t" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<std::string, uint64_t, SmallStringRNG, UniformUint64RNG>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

    {
        using MidStringRNG = StringRNG<56>;
        fprintf(stderr, "\nTest Mid String with max length 56\n\n");
        std::string data_file_name = map_name + "__" + hash_name + "__" + "mid_string_56" + "__" + "uint64_t" + ".csv";
        std::string export_file_path = data_dir_path + data_file_name;
        FILE *export_fp = fopen(export_file_path.c_str(), "w");
        if (export_fp == nullptr) {
            fprintf(stderr, "Error when create file at %s\n%s", export_file_path.c_str(), std::strerror(errno));
            return;
        }
        auto result_vec = TestOnePairType<std::string, uint64_t, MidStringRNG, UniformUint64RNG>(seed, key_size_array);
        ExportToCsv(export_fp, key_size_array, result_vec);
    }

}

int main(int argc, const char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Invalid parameters!\nUsage: bench_{map_name}__{hash_name} seed(size_t) export_data_dir");
        return -1;
    }
    size_t seed = std::stoul(std::string(argv[1]));
    BenchTest(seed, argv[2]);
    return 0;
}
