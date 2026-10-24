#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include <iostream>
using namespace ns3;

int main ()
{
  uint32_t cases = 0;
  for (uint8_t page = 0; page < 4; ++page)
    for (uint8_t bytes = 0; bytes <= 4; ++bytes)
      {
        TIM tim;
        tim.SetDTIMPeriod (1); tim.SetDTIMCount (0);
        tim.SetPageIndex (page); tim.SetPageSliceNum (31); tim.SetTafficIndicator (1);
        TIM::EncodedBlock block;
        block.SetBlockOffset(31); block.SetBlockBitmap(0x80);
        uint8_t bits = 0x80; block.SetEncodedInfo(&bits, 1);
        tim.SetPartialVBitmap(block);
        TIM snapshot = tim;
        tim.GetPartialVBitmap()[2] = 0;
        tim = snapshot;
        NS_ABORT_MSG_IF(tim.GetPartialVBitmap()[2] != 0x80, "TIM copy aliases source buffer");
        Buffer buffer; buffer.AddAtEnd (tim.GetSerializedSize ());
        tim.Serialize (buffer.Begin ());
        TIM decoded; decoded.Deserialize (buffer.Begin ());
        NS_ABORT_MSG_IF (decoded.GetPageIndex () != page || decoded.GetPageSliceNum () != 31 ||
                         decoded.GetTrafficIndicator () != 1 || decoded.GetDTIMPeriod () != 1,
                         "TIM control fields changed during serialization");

        NS_ABORT_MSG_IF(decoded.m_length != 3 || decoded.GetPartialVBitmap()[2] != 0x80,
                        "TIM block bitmap changed during serialization");
        pageSlice slice;
        slice.SetPageindex (page); slice.SetPagePeriod (4);
        slice.SetPageSliceCount (0); slice.SetPageSliceLen (1);
        slice.SetBlockOffset (0); slice.SetTIMOffset (0);
        const uint32_t bitmap = bytes == 0 ? 0 : (uint32_t (1) << (8 * bytes - 1));
        slice.SetPageBitmap (bitmap);
        pageSlice snapshotSlice = slice;
        slice.SetPageBitmap(0x80808080);
        slice = snapshotSlice;
        for (uint8_t i = 0; i < bytes; ++i)
          NS_ABORT_MSG_IF(slice.GetPageBitmap()[i] != ((bitmap >> (8 * i)) & 255),
                         "Page Slice copy aliases source buffer");
        Buffer data; data.AddAtEnd (slice.GetSerializedSize () + 4);
        auto sentinel = data.Begin (); sentinel.Next (slice.GetSerializedSize ());
        sentinel.WriteU32 (0x12345678);
        slice.Serialize (data.Begin ());
        sentinel = data.Begin (); sentinel.Next (slice.GetSerializedSize ());
        NS_ABORT_MSG_IF (sentinel.ReadU32 () != 0x12345678, "Page bitmap wrote beyond declared length");
        pageSlice restored; restored.Deserialize (data.Begin ());
        NS_ABORT_MSG_IF (restored.GetPageindex () != page || restored.GetPagePeriod () != 4 ||
                         restored.GetPageBitmapLength () != bytes, "Page Slice fields changed");
        for (uint8_t i = 0; i < bytes; ++i)
          NS_ABORT_MSG_IF (restored.GetPageBitmap ()[i] != ((bitmap >> (8 * i)) & 255), "Page bitmap changed");
        ++cases;
      }
  std::cout << "PASS: " << cases << " TIM/Page Slice round trips, including four-byte bitmaps" << std::endl;
}
