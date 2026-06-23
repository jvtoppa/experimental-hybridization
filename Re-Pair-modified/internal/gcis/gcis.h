#ifndef GCIS_H
#define GCIS_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include "../bitvector.h"
#include "../utils.h"
#include "../simple8b.h"

using namespace std;

template <typename T = uint32_t>
struct GrammarData {
    vector<uint64_t> lcps;
    vector<uint64_t> suffix_sizes;
    vector<uint64_t> suffixes;
    vector<uint64_t> level_rule_counts;
};

template <typename T = uint32_t>
class GCISCodec
{
private:
    uint64_t original_string_sz = 0;
    uint64_t alphabet_sz = 0;
    uint64_t lcps_compressed_sz = 0;
    uint64_t suffix_sizes_compressed_sz = 0;
    uint64_t rules_compressed_sz = 0;
    uint64_t cfg_compressed_sz = 0;
    uint64_t level_rule_counts_sz = 0;

    vector<uint64_t> enc_lcps;
    vector<uint64_t> enc_suf_lens;
    vector<uint64_t> enc_rules;
    vector<uint64_t> enc_level_rule_counts;

public:
     GCISCodec(GrammarData<T>&& g, size_t orig_sz, size_t alpha_sz) 
        : original_string_sz(orig_sz), alphabet_sz(alpha_sz) 
    {
       
        this->enc_lcps = move(g.lcps);
        this->enc_suf_lens = move(g.suffix_sizes);
        this->enc_rules = move(g.suffixes);
        this->enc_level_rule_counts = move(g.level_rule_counts);

        this->lcps_compressed_sz = enc_lcps.size();
        this->suffix_sizes_compressed_sz = enc_suf_lens.size();
        this->rules_compressed_sz = enc_rules.size();
        this->level_rule_counts_sz = enc_level_rule_counts.size();
    }

    explicit GCISCodec(const vector<uint64_t>& metadata_encoded) 
    {
        if (metadata_encoded.size() < 6) {
            throw runtime_error("Invalid codec vector payload: Header block is truncated.");
        }

        this->original_string_sz = metadata_encoded[0];
        this->alphabet_sz = metadata_encoded[1];
        this->lcps_compressed_sz = metadata_encoded[2];
        this->suffix_sizes_compressed_sz = metadata_encoded[3];
        this->rules_compressed_sz = metadata_encoded[4];
        this->level_rule_counts_sz = metadata_encoded[5];

        size_t expected_total_size = 6 + lcps_compressed_sz + suffix_sizes_compressed_sz + rules_compressed_sz + level_rule_counts_sz;
        if (metadata_encoded.size() < expected_total_size) {
            throw runtime_error("Corrupted payload: Data stream length is shorter than header specifications.");
        }

        auto it = metadata_encoded.begin() + 6;
        this->enc_lcps.assign(it, it + lcps_compressed_sz); it += lcps_compressed_sz;
        this->enc_suf_lens.assign(it, it + suffix_sizes_compressed_sz); it += suffix_sizes_compressed_sz;
        this->enc_rules.assign(it, it + rules_compressed_sz); it += rules_compressed_sz;
        this->enc_level_rule_counts.assign(it, it + level_rule_counts_sz);
    }

    vector<uint64_t> encode() const
    {
        vector<uint64_t> payload;
        payload.reserve(6 + lcps_compressed_sz + suffix_sizes_compressed_sz + rules_compressed_sz + cfg_compressed_sz + level_rule_counts_sz);

        payload.push_back(original_string_sz);
        payload.push_back(alphabet_sz);
        payload.push_back(lcps_compressed_sz);
        payload.push_back(suffix_sizes_compressed_sz);
        payload.push_back(rules_compressed_sz);
        payload.push_back(level_rule_counts_sz);

        payload.insert(payload.end(), enc_lcps.begin(), enc_lcps.end());
        payload.insert(payload.end(), enc_suf_lens.begin(), enc_suf_lens.end());
        payload.insert(payload.end(), enc_rules.begin(), enc_rules.end());
        payload.insert(payload.end(), enc_level_rule_counts.begin(), enc_level_rule_counts.end());
        return payload;
    }

    GrammarData<T> decode() const
    {
        GrammarData<T> decoded_grammar;
        decoded_grammar.lcps = simple8b::decode(this->enc_lcps);
        decoded_grammar.suffix_sizes = simple8b::decode(this->enc_suf_lens);
        decoded_grammar.suffixes = simple8b::decode(this->enc_rules);
        decoded_grammar.level_rule_counts = simple8b::decode(this->enc_level_rule_counts);
        
        return decoded_grammar;
    }

    uint64_t get_alphabet_size() { return alphabet_sz; }
    uint64_t get_original_string_size() { return original_string_sz; }
};

template <typename T = uint32_t>
class GCIS
{

using signed_T = typename make_signed<T>::type;

private:
    vector<T> input;
    vector<T> CFG;
    long level = 1;
    size_t alphabet_size = 256;
    bitVector types;
    vector<T> positions;
    vector<signed_T> SA; 
    vector<T> lms_distance;
    vector<T> bucket_heads;
    vector<T> bucket_tails; 
    uint64_t original_string_size;
    
    simple8b global_lcps;
    simple8b global_suffix_sizes;
    simple8b global_suffixes;
    simple8b global_level_rule_counts;
    size_t final_rules_count = 0;

    bitVector setTypes();
    vector<T> setPositions();
    void setLMS_distance();
    bool LMSequal(size_t pos1, size_t pos2);
    bool isLMS(size_t pos);
    void naming();
    void induce_l();
    void induce_s();
    void setBuckets();
    void findCFGandRules();
    void gen();

public:
    GCIS(string in, long lvl = 1) : level(lvl), types(in.size(), 0)
    {
        input.reserve(in.size() + 1);
        for (char c : in)
        {
            input.push_back(static_cast<T>(static_cast<unsigned char>(c)));
        }
        input.push_back(0);
        
        this->types = setTypes();
        this->positions = setPositions();
        this->original_string_size = in.size();
        setLMS_distance();   
        setBuckets();
    }
    
    ~GCIS() = default;    

    auto compress(function<bool()> stop_condition, vector<uint64_t>& encoded)
    {
        #ifdef EXPERIMENT
        size_t current_text_size = input.size(); 
        size_t last_rules_count = 0;
        
        cout << " Level |       Text Size       |  Rules Found   |  Encoded Size\n";
        #endif

        while(stop_condition())
        {
            gen();
            #ifdef EXPERIMENT
            size_t current_rules_count = global_lcps.size();
            size_t rules_added_this_level = current_rules_count - last_rules_count;
            last_rules_count = current_rules_count;

            size_t dictionary_bytes = global_lcps.get_current_byte_size() + global_suffix_sizes.get_current_byte_size() + global_suffixes.get_current_byte_size() + global_level_rule_counts.get_current_byte_size();

            vector<uint64_t> unsigned_cfg_data(CFG.begin(), CFG.end());
            size_t cfg_bytes = (1 + simple8b::encode(unsigned_cfg_data).size()) * sizeof(uint64_t);

            size_t level_bytes = dictionary_bytes + cfg_bytes;
            double level_mb = static_cast<double>(level_bytes) / (1000.0 * 1000.0);
            
            printf("   %2ld  |  %19zu  |  %12zu  |  %7.4f MB (%zu bytes)\n", level, current_text_size, rules_added_this_level, level_mb, level_bytes);
            #endif
            
            if (!CFG.empty()) 
            {
                vector<T>().swap(input); 
                input.reserve(CFG.size());
                
                for (auto token : CFG) 
                {
                    input.push_back(token);
                }
                #ifdef EXPERIMENT
                current_text_size = CFG.size();
                #endif
            }
        }
        auto cfg = CFG;
        encoded = encode();
        return CFG;
    }
    
    bool check_encoding()
    {
        GrammarData<T> g;
        simple8b temp_lcps = global_lcps;
        simple8b temp_sizes = global_suffix_sizes;
        simple8b temp_suf = global_suffixes;
        simple8b temp_level_counts = global_level_rule_counts;

        vector<uint64_t> compressed_lcps = temp_lcps.get_data();
        vector<uint64_t> compressed_sizes = temp_sizes.get_data();
        vector<uint64_t> compressed_suf = temp_suf.get_data();
        vector<uint64_t> compressed_levels = temp_level_counts.get_data();

        vector<uint64_t> raw_lcps = simple8b::decode(compressed_lcps);
        vector<uint64_t> raw_sizes = simple8b::decode(compressed_sizes);
        vector<uint64_t> raw_suf = simple8b::decode(compressed_suf);
        vector<uint64_t> raw_levels = simple8b::decode(compressed_levels);

        g.lcps = compressed_lcps;
        g.suffix_sizes = compressed_sizes;
        g.suffixes = compressed_suf;
        g.level_rule_counts = compressed_levels; 
        
        GCISCodec<T> codec(move(g), original_string_size, alphabet_size);
        vector<uint64_t> payload = codec.encode();

        GCISCodec<T> decoder(payload);
        GrammarData<T> gr = decoder.decode();

        return (gr.lcps == raw_lcps) && 
            (gr.suffix_sizes == raw_sizes) && 
            (gr.suffixes == raw_suf) && 
            (gr.level_rule_counts == raw_levels);
    }

    vector<uint64_t> encode()
    {
        this->final_rules_count = global_lcps.elements_size();
        GrammarData<T> g;
        
        simple8b temp_lcps = global_lcps;
        simple8b temp_sizes = global_suffix_sizes;
        simple8b temp_suf = global_suffixes;
        simple8b temp_levels = global_level_rule_counts;

        g.lcps = temp_lcps.get_data();
        g.suffix_sizes = temp_sizes.get_data();
        g.suffixes = temp_suf.get_data();
        g.level_rule_counts = temp_levels.get_data();
        
        GCISCodec<T> codec(move(g), original_string_size, this->alphabet_size);
        return codec.encode();
    }

    GrammarData<T> decode(vector<uint64_t>& encoded)
    {
        GCISCodec<T> codec(encoded);
        return codec.decode();
    }

    vector<T> getCFG() const { return CFG; }
    long getLevel() const { return level; }
    size_t getAlphabet_size() const { return alphabet_size; }

    static vector<uint64_t> decompress(const vector<uint64_t>& compressed, const vector<T>& cfg_input)
    {
        GCISCodec<T> decoder(compressed);
        GrammarData<T> grammar = decoder.decode();

        vector<uint64_t> rules;
        vector<uint64_t> rule_start_positions;
        vector<uint64_t> level_start_positions;
        size_t global_rule_idx = 0;        
        size_t cumulative_suffix_idx = 0; 

        for (size_t i = 0; i < grammar.level_rule_counts.size(); i++)
        {
            size_t no_of_rules_in_level = grammar.level_rule_counts[i];
            size_t level_start_rule_idx = global_rule_idx;
            
            level_start_positions.push_back(rule_start_positions.size());
            
            for (size_t r = 0; r < no_of_rules_in_level; r++)
            {
                if (global_rule_idx >= grammar.lcps.size()) break;

                size_t lcp = grammar.lcps[global_rule_idx];
                size_t suff_size = grammar.suffix_sizes[global_rule_idx];
                
                rule_start_positions.push_back(rules.size());

                if (r > 0 && lcp > 0) 
                {
                    size_t prev_global_rule_idx = level_start_rule_idx + (r - 1);
                    size_t prev_rule_start = rule_start_positions[prev_global_rule_idx];
                    
                    for (size_t l = 0; l < lcp; l++) 
                    {
                        rules.push_back(rules[prev_rule_start + l]);
                    }
                }

                for (size_t k = 0; k < suff_size; k++)
                {
                    if (cumulative_suffix_idx < grammar.suffixes.size()) {
                        rules.push_back(grammar.suffixes[cumulative_suffix_idx++]);
                    }
                }
                
                global_rule_idx++;
            }
        }

        vector<uint64_t> current_string(cfg_input.begin(), cfg_input.end());
        vector<uint64_t> next_string;

        for (long long r = static_cast<long long>(level_start_positions.size()) - 1; r >= 0; r--)
        {
            size_t level_start_rule = level_start_positions[r];
            size_t level_end_rule = (r + 1 < static_cast<long long>(level_start_positions.size())) 
                                    ? level_start_positions[r + 1] : rule_start_positions.size();
            size_t total_rules_this_level = level_end_rule - level_start_rule;

            next_string.clear();
            next_string.reserve(current_string.size() * 2);

            for (uint64_t token : current_string)
            {
                if (token < total_rules_this_level)
                {
                    size_t local_rule_id = token; 
                    size_t global_rule_id = level_start_rule + local_rule_id;
                    
                    size_t start_pos = rule_start_positions[global_rule_id];
                    size_t end_pos = (global_rule_id + 1 < rule_start_positions.size()) 
                                    ? rule_start_positions[global_rule_id + 1] : rules.size();

                    for (size_t idx = start_pos; idx < end_pos; idx++)
                    {
                        next_string.push_back(rules[idx]);
                    }
                }
                else
                {
                    next_string.push_back(token - total_rules_this_level);
                }
            }
            
            current_string = move(next_string);
        }

        if (!current_string.empty() && current_string.back() == 0)
        {
            current_string.pop_back();
        }

        return current_string;
    }
};

#endif // GCIS_H