#ifndef SIMPLE8B_H
#define SIMPLE8B_H

#include <vector>
#include <cstdint>
#include <algorithm>

using namespace std;

struct simple8b_selector
{
    uint32_t n_items;
    uint32_t n_bits;
    uint32_t n_waste;
};

struct selector_worker
{
    size_t packed_words;
    uint8_t selector;
};


class simple8b
{
    private:

    static constexpr simple8b_selector s8b[16] = 
    {
    {240, 0, 60}, {120, 0, 60}, {60, 1, 0},  {30, 2, 0},
    {20,  3, 0},  {15, 4, 0},   {12, 5, 0},  {10, 6, 0},
    {8, 7, 4},    {7, 8, 4},    {6, 10, 0},  {5, 12, 0},
    {4, 15, 0},   {3, 20, 0},   {2, 30, 0},  {1, 60, 0}
    };

    size_t length = 0;
    vector<uint64_t> buffer;
    vector<uint64_t> compressed;

    static uint64_t find_size(uint64_t val)
    {
        if (val == 0) return 0;
        return 64 - __builtin_clzll(val);
    }

void flush()
    {
        if (buffer.empty()) return;

        uint8_t selector = 15;
        size_t packed_words = 1;
        bool fit = true;

        for(uint8_t k = 0; k < 16; k++)
        {
            fit = true;
            const auto& config = s8b[k];
            size_t lim = min(buffer.size(), static_cast<size_t>(config.n_items));
            
            for(size_t j = 0; j < lim; j++)
            {
                if(find_size(buffer[j]) > config.n_bits) {
                    fit = false; 
                    break;
                }
            }
            
            if(fit || k == 15)
            {
                selector = k;
                packed_words = lim;
                break;
            }
        }

        uint64_t packed_word = (static_cast<uint64_t>(selector) << 60);
        uint32_t bits_per_item = s8b[selector].n_bits;

        if (bits_per_item > 0)
        {
            uint32_t shift = 0;
            for (size_t j = 0; j < packed_words; ++j)
            {
                packed_word |= (buffer[j] << shift);
                shift += bits_per_item;
            }
        }

        compressed.push_back(packed_word);
        buffer.erase(buffer.begin(), buffer.begin() + packed_words);
    }


    public:

    simple8b() = default;
    void push_back(uint64_t item)
    {
        buffer.push_back(item);
        length++;
        if(buffer.size() >= 240) flush(); 
    }

    void end_flush()
    {
        while(!buffer.empty()) flush();
    }

    size_t size(){ return length;}
    size_t elements_size(){ return compressed.size();}
    void reserve(size_t elements) { compressed.reserve(elements / 4); }
    vector<uint64_t> get_data()
    {
        end_flush(); 
        
        vector<uint64_t> result;
        result.reserve(compressed.size() + 1);
        result.push_back(length);
        result.insert(result.end(), compressed.begin(), compressed.end());
        return result;
    }

    static vector<uint64_t> encode(const vector<uint64_t>& uncompressed)
    {
        if (uncompressed.empty()) return {};

        simple8b encoder;
        
        encoder.compressed.reserve(uncompressed.size() / 4 + 1);

        for (uint64_t val : uncompressed)
        {
            encoder.push_back(val);
        }

        return encoder.get_data();
    }

    size_t simulate_flush() const
    {
        if (buffer.empty()) return 0;

        vector<uint64_t> temp_buf = buffer;
        size_t words_needed = 0;

        while (!temp_buf.empty()) {
            size_t packed_words = 1;
            for (uint8_t k = 0; k < 16; k++) {
                bool fit = true;
                const auto& config = s8b[k];
                size_t lim = min(temp_buf.size(), static_cast<size_t>(config.n_items));
                
                for (size_t j = 0; j < lim; j++) {
                    if (find_size(temp_buf[j]) > config.n_bits) {
                        fit = false; 
                        break;
                    }
                }
                if (fit || k == 15) {
                    packed_words = lim;
                    break;
                }
            }
            words_needed++;
            temp_buf.erase(temp_buf.begin(), temp_buf.begin() + packed_words);
        }
        return words_needed;
    }


    size_t get_current_byte_size() const
    {
        size_t total_64bit_words = 1 + compressed.size() + simulate_flush();
        return total_64bit_words * sizeof(uint64_t);
    }
    static vector<uint64_t> decode(const vector<uint64_t>& compressed)
    {
        vector<uint64_t> uncompressed;
        if (compressed.empty()) return {};

        size_t total = compressed[0];
        size_t c = 0;
        uncompressed.reserve(total);
        for (size_t i = 1; i < compressed.size(); ++i)
        {
            uint64_t packed_word = compressed[i];
            uint8_t selector = packed_word >> 60;
            const simple8b_selector& config = s8b[selector];
            if(config.n_bits == 0)
            {
                for(uint32_t j = 0; j < config.n_items; j++)
                {
                    if(c >= total) break;
                    uncompressed.push_back(0);
                    c++;
                }
                continue;
            }
            uint64_t mask = (config.n_bits == 64) ? ~0ULL : ((1ULL << config.n_bits) - 1);
            
            uint32_t shift = 0;
            
            for (uint32_t j = 0; j < config.n_items; ++j)
            {
                if (c >= total) break;
                uncompressed.push_back((packed_word >> shift) & mask);
                shift += config.n_bits;
                c++;
            }

        }
        return uncompressed;
    }

};

#endif