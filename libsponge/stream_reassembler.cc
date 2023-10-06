#include "stream_reassembler.hh"

#include <cassert>
#include <cstddef>
#include <utility>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassembled_bytes(0), map(), _first_unassambled_idx(0), _eof(-1) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 获取 map 中第一个大于 index 的迭代器指针
    auto pos_iter = map.upper_bound(index);
    // 尝试获取一个小于等于 index 的迭代器指针
    if (pos_iter != map.begin())
        pos_iter--;
    /**
     *  此时迭代器有三种情况，
     *  1. 一种是为 end_iter，表示不存在任何数据
     *  2. 一种是为非 end_iter，
     *      a. 一种表示确实存在一个小于等于 idx 的迭代器指针
     *      b. 一种表示当前指向的是大于 idx 的迭代器指针
     */
    size_t new_idx = index;
    if (pos_iter != map.end() && pos_iter->first <= index) {
        const size_t up_idx = pos_iter->first;
        const size_t data_size = pos_iter->second.size();
        if (index < up_idx + data_size) {
            new_idx = up_idx + data_size;
        }
    } else if (index < _first_unassambled_idx) {
        new_idx = _first_unassambled_idx;
    }
    // 子串新起始位置对应到的 data 索引
    const size_t data_start_pos = new_idx - index;
    /**
     *  NOTE: 注意，若上一个子串将当前子串完全包含，则 data_size 不为正数
     *  NOTE: 这里由于 unsigned 的结果向 signed 的变量写入，因此可能存在上溢的可能
     *        但即便上溢，最多只会造成当前传入子串丢弃，不会产生任何安全隐患
     *  PS: 而且哪个数据包会这么大，大到超过 signed long 的范围
     */
    ssize_t data_size = data.size() - data_start_pos;
    pos_iter = map.lower_bound(new_idx);
    while (pos_iter != map.end() && new_idx <= pos_iter->first) {
        const size_t data_end_pos = new_idx + data_size;
        // 如果存在重叠区域
        if (data_end_pos > pos_iter->first) {
            if (data_end_pos < pos_iter->first + pos_iter->second.size()) {
                data_size = pos_iter->first - new_idx;
                break; //!
            } else {
                _unassembled_bytes -= pos_iter->second.size();
                pos_iter = map.erase(pos_iter);
                continue;
            }
        } else
            break;
    }
    // 检测是否存在数据超出了容量。注意这里的容量并不是指可保存的字节数量，而是指可保存的窗口大小
    //! NOTE: 注意这里我们仍然接收了 index 小于 first_unacceptable_idx  但
    //        index + data.size >= first_unacceptable_idx 的那部分数据
    //        多余的那部分数据最多也只会多占用 1kb 左右，属于可承受范围之内
    size_t first_unacceptable_idx = _first_unassambled_idx + _capacity - _output.buffer_size();
    if (first_unacceptable_idx <= new_idx)
        return;

    if (data_size > 0) {
        const string new_data = data.substr(data_start_pos, data_size);
        // 如果新字串可以直接写入
        if (new_idx == _first_unassambled_idx) {
            const size_t write_byte = _output.write(new_data);
            _first_unassambled_idx += write_byte;
            // 如果没写全，则将其保存起来
            if (write_byte < new_data.size()) {
                const string data_store = new_data.substr(write_byte, new_data.size() - write_byte);
                _unassembled_bytes += data_store.size();
                map.insert(make_pair(_first_unassambled_idx, std::move(data_store)));
            }
        } else {
            const string data_store = new_data.substr(0, new_data.size());
            _unassembled_bytes += data_store.size();
            map.insert(make_pair(new_idx, std::move(data_store)));
        }
    }
    // 一定要处理重叠字串的情况
    for (auto iter = map.begin(); iter != map.end();) {
        assert(_first_unassambled_idx <= iter->first);
        if (_first_unassambled_idx == iter->first) {
            const size_t write_byte = _output.write(iter->second);
            _first_unassambled_idx += write_byte;
            // 如果没写全，则将其保存起来
            if (write_byte < iter->second.size()) {
                _unassembled_bytes += iter->second.size() - write_byte;
                map.insert(make_pair(_first_unassambled_idx, iter->second.substr(write_byte)));

                _unassembled_bytes -= iter->second.size();
                map.erase(iter);
                break;
            }
            _unassembled_bytes -= iter->second.size();
            iter = map.erase(iter);
        } else 
            break;
    }
    
    if (eof)
        _eof = index + data.size();
    if (_eof <= _first_unassambled_idx)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
