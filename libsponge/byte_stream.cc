#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buf(), _capacity(capacity), _written_size(0), _read_size(0) {}

size_t ByteStream::write(const string &data) {
    if (_end_input)
        return 0;
    size_t write_size = min(_capacity - _buf.size(), data.size());
    _written_size += write_size;
    for (auto i : data) {
        if (_buf.size() == _capacity) {
            set_error();
            return write_size;
        }
        _buf.push_back(i);
    }
    // for(size_t i = 0; i < write_size; ++i) {
    //     _buf.push_back(data[i]);
    // }
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_size = min(len, _buf.size());
    return string(_buf.begin(), _buf.begin() + peek_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = min(len, _buf.size());
    _read_size += pop_size;
    for (size_t i = 0; i < pop_size; ++i) {
        _buf.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res;
    size_t read_size = min(len, _buf.size());
    _read_size += read_size;
    for (size_t i = 0; i < read_size; ++i) {
        res.push_back(_buf.front());
        _buf.pop_front();
    }
    return res;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return _buf.empty(); }

bool ByteStream::eof() const { return _end_input && _buf.empty(); }

size_t ByteStream::bytes_written() const { return _written_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buf.size(); }
