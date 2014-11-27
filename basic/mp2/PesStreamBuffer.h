// PesStreamBuffer.h

#ifndef _JUST_DEMUX_BASIC_MP2_PES_STREAM_BUFFER_H_
#define _JUST_DEMUX_BASIC_MP2_PES_STREAM_BUFFER_H_

#include <just/data/base/DataBlock.h>

#include <streambuf>

namespace just
{
    namespace demux
    {

        class PesStreamBuffer
            : public std::basic_streambuf<boost::uint8_t>
        {
        public:
            PesStreamBuffer(
                std::basic_streambuf<boost::uint8_t> & next_layer, 
                std::vector<just::data::DataBlock> const & payloads)
                : next_layer_(next_layer)
                , payloads_(payloads)
                , ipayload_(0)
                , offset_(0)
                , left_(0)
            {
                if (payloads_.size() > 0) {
                    pos_type offset = payloads_[0].offset;
                    if (next_layer_.pubseekpos(offset, std::ios::in) == offset) {
                        left_ = payloads_[0].size;
                    }
                }
            }

        private:
            virtual std::streamsize xsgetn(
                char_type * _Ptr,
                std::streamsize _Count)
            {
                boost::uint32_t left = (boost::uint32_t)_Count;
                while (left > left_) {
                    std::streamsize n = next_layer_.sgetn(_Ptr, left_);
                    left_ -= n;
                    _Ptr += n;
                    left -= n;
                    if (left_) {
                        return _Count - left;
                    }
                    if (++ipayload_ < payloads_.size()) {
                        pos_type offset = payloads_[ipayload_].offset;
                        if (next_layer_.pubseekpos(offset, std::ios::in) == offset) {
                            left_ = payloads_[ipayload_].size;
                        } else {
                            return _Count - left;
                        }
                    } else {
                        return _Count - left;
                    }
                }
                std::streamsize n = next_layer_.sgetn(_Ptr, left);
                left_ -= n;
                left -= n;
                return _Count - left;
            }

            // for msvc
            virtual std::streamsize _Xsgetn_s(
                char_type * _Ptr,
                size_t _Ptr_size, 
                std::streamsize _Count)
            {
                return xsgetn(_Ptr, _Count);
            }

            virtual pos_type seekpos(
                pos_type position, 
                std::ios_base::openmode mode)
            {
                if (mode != std::ios_base::in) {
                    return pos_type(-1);// Ä£Ê½´íÎó
                }
                boost::uint32_t left = (boost::uint32_t)position;
                for (size_t i = 0; i < payloads_.size(); ++i) {
                    if (left <= payloads_[i].size) {
                        pos_type offset = payloads_[i].offset + left;
                        if (next_layer_.pubseekpos(offset, std::ios::in) == offset) {
                            ipayload_ = i;
                            offset_ = position;
                            left_ = payloads_[i].size - left;
                            return position;
                        } else {
                            return pos_type(-1);
                        }
                    }
                    left -= payloads_[i].size;
                }
                return pos_type(-1);
            }

        private:
            std::basic_streambuf<boost::uint8_t> & next_layer_;
            std::vector<just::data::DataBlock> const & payloads_;
            size_t ipayload_;
            boost::uint64_t offset_;
            boost::uint32_t left_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_MP2_PES_STREAM_BUFFER_H_
