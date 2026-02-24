#pragma once

#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Encode casacore `Record` objects into canonical `AipsIO` byte streams.

/// Write a casacore-compatible `Record` into an `AipsIO` byte stream.
///
/// This produces the binary format consumed by `read_aipsio_record`: a Record
/// object header, RecordDesc, recordType flag, and encoded field values.
///
/// @note `uint64_t` scalar values are not representable in casacore's DataType
/// enum and will throw. `list_ptr` alternatives also throw as they have no
/// casacore wire encoding.
///
/// @note Bool arrays decoded by `read_aipsio_record` become `int32_array` and
/// are indistinguishable from genuine int32 arrays. The writer emits them as
/// `Array<Int>`, not `Array<Bool>`.
///
/// @throws std::runtime_error on unsupported value types, nesting depth
/// exceeded, or when counts/dimensions cannot be represented in the casacore
/// wire ranges (`uInt`/`Int`).
void write_aipsio_record(AipsIoWriter& writer, const Record& record);

} // namespace casacore_mini
