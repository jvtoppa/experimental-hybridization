/*
 * This file is part of Re-Pair.
 * Copyright (c) by
 * Nicola Prezza <nicola.prezza@gmail.com>, Philip Bille, and Inge Li Gørtz
 *
 * Re-Pair is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Re-Pair is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details (<http://www.gnu.org/licenses/>).
 *
 * text_positions.hpp
 *
 * Created on: Jan 20, 2017
 * Author: nico
 *
 * array of text positions. Can be sorted by character pairs
 *
 */

#ifndef INTERNAL_TEXT_POSITIONS_HPP_
#define INTERNAL_TEXT_POSITIONS_HPP_

#include <algorithm>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cassert>
#include <unordered_map>
#include "skippable_text.hpp"

using namespace std;

template<typename itype = uint32_t, typename ctype = uint32_t, typename ll_el_t = ll_el32_t>
class text_positions {

public:

    using el_type = ll_el_t;
    using ipair = pair<itype, itype>;
    using cpair = pair<ctype, ctype>;

    /*
     * build new array of text positions with only text positions of pairs with
     * frequency at least min_freq
     */
    text_positions(skippable_text<itype, ctype>* T, itype min_freq) {
        
        this->T = T;
        assert(T->size() > 1);

        uint64_t max_sym = uint64_t(T->get_max_symbol() + 1);
        uint64_t maxd = std::max(uint64_t(std::pow(T->size(), 0.4)), max_sym);
        
        // Hard cap to prevent Out-Of-Memory on large alphabets
        const uint64_t MAX_DENSE_ALPHABET = 2000;

        if (maxd <= MAX_DENSE_ALPHABET) {
            // hash to accelerate pair sorting for small alphabets
            H = vector<vector<ipair>>(maxd, vector<ipair>(maxd, {0, 0}));
        } else {
            // Leave empty to force O(N) Radix sort fallback
            H = vector<vector<ipair>>(); 
        }

        // --- Cache-Friendly Frequency Counting ---
        vector<Element> all_pairs;
        all_pairs.reserve(T->size() - 1);

        for (itype i = 0; i < T->size() - 1; ++i) {
            if (!T->is_blank(i)) {
                cpair ab = T->pair_starting_at(i);
                if (ab != T->blank_pair() && ab != nullpair) {
                    all_pairs.push_back({ab, i});
                }
            }
        }

        // Initial constructor sort runs once
        std::sort(all_pairs.begin(), all_pairs.end());

        TP.clear();

        // Filter and insert into TP groups of freq >= min_freq
        size_t n = all_pairs.size();
        size_t start_idx = 0;
        
        while (start_idx < n) {
            size_t end_idx = start_idx + 1;
            while (end_idx < n && all_pairs[end_idx].p == all_pairs[start_idx].p) {
                end_idx++;
            }

            itype freq = end_idx - start_idx;
            if (freq >= min_freq) {
                for (size_t k = start_idx; k < end_idx; ++k) {
                    TP.push_back(all_pairs[k].pos);
                }
            }
            start_idx = end_idx;
        }
    }

    /*
     * fill TP with non-blank text positions
     *
     * WARNING: this function does not sort text positions
     */
    void fill_with_text_positions() {
        assert(T->number_of_non_blank_characters() > 1);

        TP = vector<itype>(0); // free memory
        TP = vector<itype>(T->number_of_non_blank_characters() - 1);

        itype j = 0;
        for (itype i = 0; i < T->size(); ++i) {
            if (not T->is_blank(i) and j < TP.size()) {
                TP[j++] = i;
            }
        }
    }

    /*
     * get i-th text position
     */
    itype operator[](itype i) {
        assert(i < size());
        return TP[i];
    }

    void nlogn_sort(itype i, itype j) {
        std::sort(TP.begin() + i, TP.begin() + j, comparator(T));
    }

    /*
     * cluster TP[i,...,j-1] by character pairs. 
     * Uses fast direct hash for small symbols, O(N) Radix sort for large.
     */
    void cluster(itype i, itype j) {
        
        // Fallback for large alphabets: strict cache-friendly O(N) Radix Sort
        if (H.empty() || T->get_max_symbol() >= H.size()) {
            fast_sort_cluster(i, j);
            assert(is_clustered(i, j));
            return;
        }

        assert(i < size());
        assert(j <= size());
        assert(i < j);

        // mark in a bitvector only one position per distinct pair
        auto distinct_pair_positions = vector<bool>(j - i, false);

        // first step: count frequencies
        for (itype k = i; k < j; ++k) {
            if (not T->is_blank(TP[k])) {
                cpair ab = T->pair_starting_at(TP[k]);
                ctype a = ab.first;
                ctype b = ab.second;

                if (ab != nullpair) {
                    assert(a < H.size());
                    assert(b < H.size());

                    distinct_pair_positions[k - i] = (H[a][b].first == 0);
                    H[a][b].first++;
                }
            }
        }

        itype t = i; // cumulated freq

        // second step: cumulate frequencies
        for (itype k = i; k < j; ++k) {
            if (distinct_pair_positions[k - i]) {
                cpair ab = T->pair_starting_at(TP[k]);
                ctype a = ab.first;
                ctype b = ab.second;

                assert(ab != nullpair);
                assert(a < H.size());
                assert(b < H.size());

                itype temp = H[a][b].first;
                H[a][b].first = t;
                H[a][b].second = t;
                t += temp;
            }
        }

        for (itype k = i; k < j; ++k) distinct_pair_positions[k - i] = false;

        itype null_start = t;
        itype k = i; 

        while (k < j) {
            cpair ab = T->pair_starting_at(TP[k]);
            ctype a = ab.first;
            ctype b = ab.second;

            itype ab_start, ab_end;

            if (ab == nullpair) {
                ab_start = null_start;
                ab_end = t;
            } else {
                ab_start = H[a][b].first;
                ab_end = H[a][b].second;
            }

            if (k >= ab_start and k <= ab_end) {
                distinct_pair_positions[k - i] = (k == ab_start and ab != nullpair);
                k++;
                if (ab == nullpair) {
                    t += (ab_end == k);
                } else {
                    H[a][b].second += (ab_end == k);
                }
            } else {
                itype temp = TP[k];
                TP[k] = TP[ab_end];
                TP[ab_end] = temp;

                if (ab == nullpair) {
                    t++;
                } else {
                    H[a][b].second++;
                }
            }
        }

        // restore H
        for (itype k = i; k < j; ++k) {
            if (distinct_pair_positions[k - i]) {
                cpair ab = T->pair_starting_at(TP[k]);
                H[ab.first][ab.second] = {0, 0};
            }
        }

        assert(is_clustered(i, j));
    }

    void cluster() {
        cluster(0, size());
    }

    void nlogn_sort() {
        nlogn_sort(0, size());
    }

    itype size() {
        return TP.size();
    }

    /*
     * check that TP[i,...,j-1] is clustered by character pairs
     */
    bool is_clustered(itype i, itype j) {
        if (j <= i) return true;

        itype m = j - i;
        auto V = unordered_map<cpair, bool>(2 * m);

        for (itype k = i + 1; k < j; ++k) {
            if (T->pair_starting_at(TP[k]) != T->pair_starting_at(TP[k - 1])) {
                auto p = T->pair_starting_at(TP[k - 1]);
                if (V.count(p) == 0) {
                    V.insert({p, true});
                } else {
                    return false;
                }
            }
        }

        auto p = T->pair_starting_at(TP[j - 1]);
        if (V.count(p) != 0) return false;

        return true;
    }

    bool contains_only(itype i, itype j, cpair ab) {
        for (itype k = i; k < j; ++k) {
            if (T->pair_starting_at(TP[k]) != ab) return false;
        }
        return true;
    }

private:

    struct comparator {
        comparator(skippable_text<itype, ctype>* T) {
            this->T = T;
        }
        bool operator()(int i, int j) {
            return T->pair_starting_at(i) < T->pair_starting_at(j);
        }
        skippable_text<itype, ctype>* T;
    };

    struct Element {
        cpair p;
        itype pos;
        bool operator<(const Element& other) const {
            if (p != other.p) return p < other.p;
            return pos < other.pos;
        }
    };

    /*
     * Linear time O(N) clustering using 64-bit pair packing and LSD Radix Sort.
     * Safely filters out blanks/invalid pairs to ensure exact frequency matching.
     */
    void fast_sort_cluster(itype i, itype j) {
        if (j <= i) return;
        
        size_t len = j - i;

        if (sizeof(ctype) > 4) {
            vector<Element> buffer(len);
            for (size_t k = 0; k < len; ++k) {
                buffer[k] = {T->pair_starting_at(TP[i + k]), TP[i + k]};
            }
            std::sort(buffer.begin(), buffer.end());
            for (size_t k = 0; k < len; ++k) {
                TP[i + k] = buffer[k].pos;
            }
            return;
        }

        // --- Radix Sort for 32-bit ctype ---
        vector<uint64_t> packed_buffer(len);
        vector<itype> pos_buffer(len);
        
        // 1. Pack data into arrays while filtering out blanks
        for (size_t k = 0; k < len; ++k) {
            itype text_pos = TP[i + k];
            
            // CRITICAL FIX: If the position is blank, treat it strictly as nullpair
            // to prevent dead characters from inflating real pair frequencies.
            if (T->is_blank(text_pos)) {
                packed_buffer[k] = ~uint64_t(0); // 0xFFFFFFFFFFFFFFFF
            } else {
                cpair p = T->pair_starting_at(text_pos);
                if (p == nullpair) {
                    packed_buffer[k] = ~uint64_t(0);
                } else {
                    packed_buffer[k] = (static_cast<uint64_t>(p.first) << 32) | static_cast<uint32_t>(p.second);
                }
            }
            pos_buffer[k] = text_pos;
        }
        
        // 2. LSD Radix Sort (Base 256 for 8 passes over 64 bits)
        vector<uint64_t> temp_packed(len);
        vector<itype> temp_pos(len);
        
        for (int shift = 0; shift < 64; shift += 8) {
            int count[256] = {0};
            
            // Count frequencies
            for (size_t k = 0; k < len; ++k) {
                count[(packed_buffer[k] >> shift) & 0xFF]++;
            }
            
            // Compute offsets
            int offset = 0;
            for (int c = 0; c < 256; ++c) {
                int temp = count[c];
                count[c] = offset;
                offset += temp;
            }
            
            // Route elements
            for (size_t k = 0; k < len; ++k) {
                int byte = (packed_buffer[k] >> shift) & 0xFF;
                int dest = count[byte]++;
                temp_packed[dest] = packed_buffer[k];
                temp_pos[dest] = pos_buffer[k];
            }
            
            packed_buffer.swap(temp_packed);
            pos_buffer.swap(temp_pos);
        }
        
        // 3. Write back sorted positions to TP
        for (size_t k = 0; k < len; ++k) {
            TP[i + k] = pos_buffer[k];
        }
    }

    skippable_text<itype, ctype>* T;

    // hash to speed-up pair sorting (to linear time) for small alphabets only
    vector<vector<ipair>> H; 

    vector<itype> TP;

    const itype null = ~itype(0);
    const cpair nullpair = {null, null};

};

// Specialized Types
typedef text_positions<uint32_t, uint32_t, ll_el32_t> text_positions32_t;
typedef text_positions<uint64_t, uint64_t, ll_el64_t> text_positions64_t;

#endif /* INTERNAL_TEXT_POSITIONS_HPP_ */