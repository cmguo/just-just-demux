// PesAdtsSplitter.h

#ifndef _PPBOX_DEMUX_BASIC_MP2_PES_ADTS_SPLITTER_H_
#define _PPBOX_DEMUX_BASIC_MP2_PES_ADTS_SPLITTER_H_

#include "ppbox/demux/basic/mp2/PesStreamBuffer.h"

#include <ppbox/avcodec/aac/AacAdts.h>
#include <ppbox/avbase/stream/BitsIStream.h>

namespace ppbox
{
    namespace demux
    {

        struct PesAdtsSplitter
        {
            PesAdtsSplitter()
                : has_header_(false)
                , size_(7)
            {
            }

            bool finish(
                ppbox::avformat::Mp2IArchive & ar, 
                std::vector<ppbox::data::DataBlock> & payloads, 
                boost::uint32_t size)
            {
                if (size < size_) {
                    return false;
                }
                if (!has_header_) {
                    PesStreamBuffer buffer(*ar.rdbuf(), payloads);
                    ppbox::avbase::BitsIStream<boost::uint8_t> ia(buffer);
                    ia >> header_;
                    assert(ia);
                    size_ = header_.frame_length;
                    if (size < size_) {
                        return false;
                    }
                }
                save_size_ = size - size_;
                size = size_;
                size_t i = 0;
                while (size && size >= payloads[i].size) {
                    size -= payloads[i].size;
                    ++i;
                }
                if (size) {
                    save_payloads_.push_back(ppbox::data::DataBlock(payloads[i].offset + size, payloads[i].size - size));
                    payloads[i].size = size;
                    ++i;
                }
                for (size_t j = i; j < payloads.size(); ++j) {
                    save_payloads_.push_back(payloads[i]);
                }
                payloads.resize(i);
                return true;
            }

            void pop_payload()
            {
                save_size_ -= save_payloads_.back().size;
                save_payloads_.pop_back();
            }

            void clear(
                std::vector<ppbox::data::DataBlock> & payloads)
            {
                payloads.swap(save_payloads_);
                has_header_ = false;
                size_ = 7;
            }

            boost::uint32_t save_size() const
            {
                return save_size_;
            }

            boost::uint32_t size() const
            {
                return size_;
            }

        private:
            ppbox::avcodec::AacAdts header_;
            std::vector<ppbox::data::DataBlock> save_payloads_;
            boost::uint32_t save_size_;
            bool has_header_;
            boost::uint32_t size_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP2_PES_ADTS_SPLITTER_H_
