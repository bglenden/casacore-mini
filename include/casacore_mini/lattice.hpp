// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"
#include "casacore_mini/table.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Lattice class hierarchy: abstract base, in-memory, paged, sub, temp.
///
/// All lattice data is stored in Fortran (column-major) order and accessed
/// through `LatticeArray<T>` which provides `std::mdspan` views.

// ── Forward declarations ───────────────────────────────────────────────

template <typename T> class Lattice;
template <typename T> class MaskedLattice;
template <typename T> class ArrayLattice;
template <typename T> class PagedArray;
template <typename T> class SubLattice;
template <typename T> class TempLattice;
template <typename T> class LatticeConcat;
template <typename T> class RebinLattice;
template <typename T> class LatticeIterator;

// ── Lattice<T> ─────────────────────────────────────────────────────────

/// <summary>
/// Abstract base class for all N-dimensional array (lattice) types.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// Lattice<T> defines the minimal interface for reading and writing
/// multidimensional data in casacore-mini.  All data is stored and
/// accessed in Fortran order (first axis varies fastest), consistent
/// with casacore's array convention.
///
/// Concrete implementations include:
/// <ul>
///   <li> ArrayLattice<T>  — all data in memory
///   <li> PagedArray<T>    — data on disk in a casacore table
///   <li> TempLattice<T>   — memory for small data, disk for large
///   <li> SubLattice<T>    — windowed view of another lattice
///   <li> LatticeConcat<T> — concatenation of multiple lattices
///   <li> RebinLattice<T>  — integer-factor downsampled view
/// </ul>
///
/// The lattice hierarchy mirrors casacore's <src>Lattice</src> /
/// <src>MaskedLattice</src> / <src>ImageInterface</src> design but
/// strips AIPS++-era complexity in favour of modern C++17 idioms.
///
/// <b>Read/write contract:</b> before calling any mutating method
/// (<src>put</src>, <src>put_slice</src>, <src>put_at</src>,
/// <src>set</src>, <src>apply</src>), callers must verify
/// <src>is_writable()</src> returns true.  Read-only lattice types
/// (SubLattice constructed from a const parent, LatticeConcat,
/// RebinLattice) will throw <src>std::runtime_error</src> on write
/// attempts.
///
/// <b>Ownership:</b> Lattice does not own its data by default; only
/// ArrayLattice and PagedArray are primary owners.  SubLattice,
/// LatticeConcat, and RebinLattice hold non-owning pointers to their
/// parent(s) and must not outlive them.
/// </synopsis>
///
/// <example>
/// Read and modify an in-memory lattice:
/// <srcblock>
///   using namespace casacore_mini;
///   IPosition shape{64, 64, 16};
///   ArrayLattice<float> lat(shape, 0.0f);
///
///   // Write a single pixel.
///   lat.put_at(1.5f, IPosition{10, 20, 5});
///
///   // Read a slice.
///   Slicer sl(IPosition{0, 0, 0}, IPosition{8, 8, 1});
///   auto chunk = lat.get_slice(sl);
///
///   // Apply a function element-wise (squares every pixel).
///   lat.apply([](float x) { return x * x; });
/// </srcblock>
/// </example>
///
/// <motivation>
/// A uniform abstract interface allows pipeline algorithms to be written
/// once and applied to in-memory, disk-backed, sub-region, concatenated,
/// or rebinned arrays without modification, mirroring casacore's approach
/// to data abstraction.
/// </motivation>
///
/// <prerequisite>
///   <li> LatticeArray<T> — concrete owning array type
///   <li> IPosition       — shape and index vector
///   <li> Slicer          — sub-region descriptor
/// </prerequisite>
template <typename T>
class Lattice {
  public:
    virtual ~Lattice() = default;

    /// Shape of the lattice.
    [[nodiscard]] virtual const IPosition& shape() const = 0;
    /// Number of dimensions.
    [[nodiscard]] std::size_t ndim() const { return shape().ndim(); }
    /// Total number of elements.
    [[nodiscard]] std::int64_t nelements() const { return shape().product(); }

    /// Whether the lattice supports writing.
    [[nodiscard]] virtual bool is_writable() const = 0;
    /// Whether the lattice is backed by persistent storage.
    [[nodiscard]] virtual bool is_paged() const { return false; }

    /// Read the entire lattice into a LatticeArray.
    [[nodiscard]] virtual LatticeArray<T> get() const = 0;
    /// Read a slice of the lattice.
    [[nodiscard]] virtual LatticeArray<T> get_slice(const Slicer& slicer) const = 0;
    /// Read a single element.
    [[nodiscard]] virtual T get_at(const IPosition& where) const = 0;

    /// Write the entire lattice from a LatticeArray.
    virtual void put(const LatticeArray<T>& data) = 0;
    /// Write a slice into the lattice.
    virtual void put_slice(const LatticeArray<T>& data, const Slicer& slicer) = 0;
    /// Write a single element.
    virtual void put_at(const T& value, const IPosition& where) = 0;

    /// Set all elements to the given value.
    virtual void set(const T& value);

    /// Apply a function element-wise (in-place).
    virtual void apply(const std::function<T(T)>& func);

  protected:
    Lattice() = default;
    Lattice(const Lattice&) = default;
    Lattice& operator=(const Lattice&) = default;
    Lattice(Lattice&&) = default;
    Lattice& operator=(Lattice&&) = default;
};

// ── MaskedLattice<T> ──────────────────────────────────────────────────

/// <summary>
/// Abstract lattice that also exposes a boolean pixel mask.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// MaskedLattice<T> extends <src>Lattice<T></src> with methods for
/// reading a per-pixel validity mask.  The mask convention follows
/// casacore: <b>true means the pixel is valid</b> (unmasked); false
/// means the pixel is flagged and should be ignored by algorithms.
///
/// Not all MaskedLattice implementations carry an explicit mask.
/// <src>has_pixel_mask()</src> returns false for lattice types that
/// always treat every pixel as valid (ArrayLattice, PagedArray, etc.).
/// In that case <src>get_mask()</src> returns an all-true array of the
/// appropriate shape.
///
/// Implementations that do carry a mask (e.g. casacore images with
/// OTF masks) override <src>has_pixel_mask()</src> to return true and
/// provide genuine data from <src>get_mask()</src>.
/// </synopsis>
///
/// <note role="caution">
/// casacore's mask convention is the logical inverse of FITS BLANK or
/// IEEE NaN masking: a stored value of <b>true</b> indicates a good
/// (not blanked) pixel, while <b>false</b> marks a flagged pixel.
/// </note>
template <typename T>
class MaskedLattice : public Lattice<T> {
  public:
    /// Whether the lattice has a pixel mask.
    [[nodiscard]] virtual bool is_masked() const { return has_pixel_mask(); }
    /// Whether a pixel mask is attached.
    [[nodiscard]] virtual bool has_pixel_mask() const = 0;
    /// Get the full pixel mask (true = valid).
    [[nodiscard]] virtual LatticeArray<bool> get_mask() const = 0;
    /// Get a slice of the pixel mask.
    [[nodiscard]] virtual LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const = 0;
};

// ── ArrayLattice<T> ───────────────────────────────────────────────────

/// <summary>
/// In-memory lattice backed by a LatticeArray<T>.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// ArrayLattice<T> is the simplest concrete Lattice: all data lives in
/// a <src>LatticeArray<T></src> held as a member.  It is always writable
/// and never has a pixel mask (every pixel is implicitly valid).
///
/// Because the underlying <src>LatticeArray<T></src> uses copy-on-write
/// storage, constructing an ArrayLattice from an existing LatticeArray
/// is cheap (shared pointer copy) until the first mutation.
///
/// Use ArrayLattice when:
/// <ul>
///   <li> The entire dataset fits comfortably in RAM.
///   <li> Random access to individual elements is required.
///   <li> You need an mdspan view of the data (via <src>array().const_view<N>()</src>).
/// </ul>
///
/// For data larger than available RAM, prefer <src>TempLattice<T></src>
/// (which transparently spills to disk) or <src>PagedArray<T></src>.
/// </synopsis>
///
/// <example>
/// Creating and slicing an in-memory lattice:
/// <srcblock>
///   using namespace casacore_mini;
///
///   ArrayLattice<float> lat(IPosition{256, 256, 4}, 0.0f);
///   lat.put_at(42.0f, IPosition{128, 128, 2});
///
///   Slicer sl(IPosition{0, 0, 2}, IPosition{256, 256, 1});
///   auto plane = lat.get_slice(sl);    // 256 x 256 x 1 LatticeArray
///
///   // Access the raw array for mdspan work.
///   const auto& arr = lat.array();
///   auto v = arr.const_view<3>();      // 3-D mdspan
/// </srcblock>
/// </example>
template <typename T>
class ArrayLattice : public MaskedLattice<T> {
  public:
    /// Construct from shape (value-initialized).
    explicit ArrayLattice(IPosition shape);
    /// Construct from shape and initial value.
    ArrayLattice(IPosition shape, T value);
    /// Construct from existing LatticeArray.
    explicit ArrayLattice(LatticeArray<T> data);

    [[nodiscard]] const IPosition& shape() const override { return data_.shape(); }
    [[nodiscard]] bool is_writable() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override { return data_; }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Direct access to the underlying LatticeArray.
    [[nodiscard]] const LatticeArray<T>& array() const { return data_; }
    [[nodiscard]] LatticeArray<T>& array() { return data_; }

  private:
    LatticeArray<T> data_;
};

// ── PagedArray<T> ─────────────────────────────────────────────────────

/// <summary>
/// Disk-backed lattice stored as a single-column casacore table.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// PagedArray<T> persists lattice data in a casacore-format table
/// directory on disk.  The table schema mirrors casacore's
/// <src>PagedArray</src> layout: a table with one array column named
/// "map" backed by a tiled storage manager (TiledShapeStMan).
///
/// Two construction modes are available:
/// <ul>
///   <li> Create a new table: <src>PagedArray(shape, path)</src>.
///        The path must not already exist.
///   <li> Open an existing table: <src>PagedArray(path, writable)</src>.
///        Pass <src>writable = true</src> to enable writes.
/// </ul>
///
/// <src>get()</src> reads the entire array into a LatticeArray in
/// memory.  For large arrays prefer <src>get_slice()</src> combined
/// with <src>LatticeIterator<T></src> to process data in tiles.
///
/// Call <src>flush()</src> to ensure all pending writes reach disk
/// before the object is destroyed or the path is handed to another
/// process.
/// </synopsis>
///
/// <example>
/// Creating and reading back a paged array:
/// <srcblock>
///   using namespace casacore_mini;
///   namespace fs = std::filesystem;
///
///   fs::path p = "/tmp/my_paged_array";
///   {
///       PagedArray<float> pa(IPosition{1024, 1024, 64}, p);
///       pa.set(0.0f);
///       pa.put_at(1.0f, IPosition{512, 512, 32});
///       pa.flush();
///   }
///
///   // Re-open read-only.
///   PagedArray<float> pa2(p);
///   float val = pa2.get_at(IPosition{512, 512, 32});
///   assert(val == 1.0f);
/// </srcblock>
/// </example>
///
/// <note role="caution">
/// PagedArray does not delete the on-disk table when the C++ object is
/// destroyed.  Callers are responsible for removing the table directory
/// when it is no longer needed (e.g. via <src>Table::drop_table()</src>).
/// </note>
template <typename T>
class PagedArray : public MaskedLattice<T> {
  public:
    /// Create a new paged array on disk.
    PagedArray(IPosition shape, const std::filesystem::path& path);
    /// Open an existing paged array from disk (read-only by default).
    explicit PagedArray(const std::filesystem::path& path,
                        bool writable = false);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return writable_; }
    [[nodiscard]] bool is_paged() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Flush pending writes to disk.
    void flush();
    /// Table name (directory basename).
    [[nodiscard]] std::string table_name() const;
    /// Path to the table directory.
    [[nodiscard]] const std::filesystem::path& table_path() const;

  private:
    IPosition shape_;
    std::filesystem::path path_;
    bool writable_ = false;
    mutable std::shared_ptr<Table> table_;
};

// ── SubLattice<T> ─────────────────────────────────────────────────────

/// <summary>
/// A non-owning view into a sub-region of another lattice.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// SubLattice<T> presents a windowed, optionally strided, view of a
/// parent <src>Lattice<T></src>.  It does not own any data; all reads
/// and writes are forwarded to the parent after translating local
/// coordinates into the parent coordinate space.
///
/// The sub-region is specified by a <src>Slicer</src> supplied at
/// construction time.  The SubLattice shape equals
/// <src>slicer.length()</src>.
///
/// Two constructors are provided:
/// <ul>
///   <li> Mutable: <src>SubLattice(Lattice<T>&, Slicer)</src>.
///        Writable only if the parent is writable.
///   <li> Read-only: <src>SubLattice(const Lattice<T>&, Slicer)</src>.
///        Always read-only regardless of the parent's writability.
/// </ul>
///
/// Coordinate mapping:  for a local index <src>l[d]</src> on axis d,
/// the corresponding parent index is
/// <src>slicer.start()[d] + l[d] * slicer.stride()[d]</src>.
/// </synopsis>
///
/// <example>
/// Extracting a writable sub-region:
/// <srcblock>
///   using namespace casacore_mini;
///
///   ArrayLattice<float> parent(IPosition{512, 512, 32}, 0.0f);
///
///   // View the first 64 x 64 x 1 region of plane 10.
///   Slicer sl(IPosition{0, 0, 10}, IPosition{64, 64, 1});
///   SubLattice<float> sub(parent, sl);
///
///   sub.put_at(7.0f, IPosition{0, 0, 0});
///   // Equivalent to: parent.put_at(7.0f, IPosition{0, 0, 10})
/// </srcblock>
/// </example>
///
/// <note role="caution">
/// SubLattice holds a raw pointer to its parent.  The parent must
/// remain alive for the entire lifetime of the SubLattice; use-after-free
/// is undefined behaviour.
/// </note>
template <typename T>
class SubLattice : public MaskedLattice<T> {
  public:
    /// Construct from a lattice and a slicer defining the sub-region.
    SubLattice(Lattice<T>& parent, Slicer slicer);
    /// Construct a read-only sub-lattice.
    SubLattice(const Lattice<T>& parent, Slicer slicer);

    [[nodiscard]] const IPosition& shape() const override { return sub_shape_; }
    [[nodiscard]] bool is_writable() const override { return writable_; }
    [[nodiscard]] bool is_paged() const override { return parent_->is_paged(); }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

  private:
    /// Map a local index into the parent coordinate space.
    [[nodiscard]] IPosition to_parent_index(const IPosition& local) const;
    /// Map a local slicer into the parent coordinate space.
    [[nodiscard]] Slicer to_parent_slicer(const Slicer& local) const;

    Lattice<T>* parent_rw_ = nullptr;
    const Lattice<T>* parent_ = nullptr;
    Slicer region_;
    IPosition sub_shape_;
    bool writable_ = false;
};

// ── TempLattice<T> ───────────────────────────────────────────────────

/// <summary>
/// Temporary lattice that stores data in memory for small arrays and
/// transparently spills to disk for large ones.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// TempLattice<T> chooses its backing store at construction time:
///
/// <ul>
///   <li> If <src>shape.product() * sizeof(T) <= max_memory_bytes</src>,
///        a <src>ArrayLattice<T></src> is used (all data in RAM).
///   <li> Otherwise, a <src>PagedArray<T></src> in a system-designated
///        temporary directory is used.
/// </ul>
///
/// The default memory limit is 16 MiB, which is appropriate for
/// moderate-sized intermediate arrays in image-plane algorithms.
/// Adjust <src>max_memory_bytes</src> based on the available system
/// RAM and the size of the data being processed.
///
/// The temporary table (if created) is stored under
/// <src>std::filesystem::temp_directory_path() / "casacore_mini_tmp"</src>
/// with a hash-derived filename.  It is <b>not</b> automatically
/// deleted when the TempLattice is destroyed; callers should arrange
/// cleanup if the temporary directory must not persist.
///
/// All public methods delegate to the internal implementation
/// (<src>ArrayLattice</src> or <src>PagedArray</src>) transparently,
/// so callers need not distinguish between the two cases.
/// </synopsis>
///
/// <example>
/// Using TempLattice as an intermediate work buffer:
/// <srcblock>
///   using namespace casacore_mini;
///
///   // Automatically uses RAM for small arrays, disk for large ones.
///   TempLattice<float> work(IPosition{4096, 4096, 1});
///   work.set(0.0f);
///
///   if (work.is_paged()) {
///       // Large array — was spilled to disk.
///   } else {
///       // Small array — lives in RAM.
///   }
/// </srcblock>
/// </example>
template <typename T>
class TempLattice : public MaskedLattice<T> {
  public:
    /// Construct with given shape. If size > max_memory_bytes, use disk.
    TempLattice(IPosition shape, std::size_t max_memory_bytes = 16 * 1024 * 1024);

    [[nodiscard]] const IPosition& shape() const override { return impl_->shape(); }
    [[nodiscard]] bool is_writable() const override { return true; }
    [[nodiscard]] bool is_paged() const override { return impl_->is_paged(); }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override { return impl_->get(); }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override {
        return impl_->get_slice(slicer);
    }
    [[nodiscard]] T get_at(const IPosition& where) const override {
        return impl_->get_at(where);
    }

    void put(const LatticeArray<T>& data) override { impl_->put(data); }
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override {
        impl_->put_slice(data, slicer);
    }
    void put_at(const T& value, const IPosition& where) override {
        impl_->put_at(value, where);
    }

  private:
    std::unique_ptr<Lattice<T>> impl_;
};

// ── LatticeConcat<T> ──────────────────────────────────────────────────

/// <summary>
/// Read-only logical concatenation of multiple lattices along one axis.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// LatticeConcat<T> presents two or more lattices as a single larger
/// lattice joined along a specified axis without copying data.  Only
/// the shapes of the constituent lattices along the concatenation axis
/// are summed; all other axes must have identical extents.
///
/// For example, concatenating three lattices of shape {64, 64, 16}
/// along axis 2 yields a LatticeConcat of shape {64, 64, 48}.
///
/// Reads are dispatched to the appropriate component lattice based on
/// the position along the concatenation axis.  Slices that span a
/// component boundary are handled by reading from the full concatenated
/// array (<src>get()</src>) and then slicing the result.
///
/// LatticeConcat is read-only: calls to <src>put()</src>,
/// <src>put_slice()</src>, or <src>put_at()</src> throw
/// <src>std::runtime_error</src>.
///
/// The component lattice pointers must remain valid for the lifetime of
/// the LatticeConcat object.
/// </synopsis>
///
/// <example>
/// Concatenating spectral planes:
/// <srcblock>
///   using namespace casacore_mini;
///
///   ArrayLattice<float> a(IPosition{64, 64, 10}, 1.0f);
///   ArrayLattice<float> b(IPosition{64, 64, 10}, 2.0f);
///   ArrayLattice<float> c(IPosition{64, 64,  5}, 3.0f);
///
///   // Concatenate along axis 2 (channel axis).
///   LatticeConcat<float> cat(
///       {&a, &b, &c},   // raw pointers; a, b, c must outlive cat
///       2);
///   assert(cat.shape() == (IPosition{64, 64, 25}));
///
///   // Read pixel from the second component.
///   float v = cat.get_at(IPosition{0, 0, 15}); // in component b
/// </srcblock>
/// </example>
///
/// <note role="caution">
/// LatticeConcat holds raw (non-owning) pointers to the component
/// lattices.  If any component is destroyed before the LatticeConcat,
/// subsequent reads will produce undefined behaviour.
/// </note>
template <typename T>
class LatticeConcat : public MaskedLattice<T> {
  public:
    /// Construct from a list of lattices and the axis along which to
    /// concatenate.
    LatticeConcat(std::vector<const Lattice<T>*> lattices, std::size_t axis);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Number of component lattices.
    [[nodiscard]] std::size_t nlattices() const { return lattices_.size(); }
    /// Concatenation axis index.
    [[nodiscard]] std::size_t axis() const { return axis_; }

  private:
    /// Find which component lattice contains `global_pos` on the concat axis.
    /// Returns (lattice_index, local_pos_on_axis).
    [[nodiscard]] std::pair<std::size_t, std::int64_t>
    locate(std::int64_t global_pos) const;

    std::vector<const Lattice<T>*> lattices_;
    std::size_t axis_;
    IPosition shape_;
    /// Cumulative offsets on the concat axis: offsets_[i] is the start index
    /// of lattice i in the concatenated shape.
    std::vector<std::int64_t> offsets_;
};

// ── RebinLattice<T> ──────────────────────────────────────────────────

/// <summary>
/// Read-only downsampling view of a lattice by integer rebin factors.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// RebinLattice<T> presents a coarser version of a parent lattice.
/// Each output pixel is the arithmetic mean of a rectangular bin of
/// input pixels whose size on axis d is <src>factors[d]</src>.
///
/// The output shape on axis d is
/// <src>ceil(parent_shape[d] / factors[d])</src>.  If the parent extent
/// is not evenly divisible by the factor, the last bin on that axis is
/// smaller, averaging only the remaining input pixels.
///
/// All rebin factors must be positive (>= 1).  A factor of 1 on an
/// axis leaves that axis unchanged.
///
/// RebinLattice is read-only: calls to <src>put()</src>,
/// <src>put_slice()</src>, or <src>put_at()</src> throw
/// <src>std::runtime_error</src>.
///
/// <b>Performance note:</b> <src>get()</src> reads the entire parent
/// lattice into memory and then computes the rebin.  For large lattices,
/// use <src>get_at()</src> or <src>get_slice()</src> to limit the
/// working set.
/// </synopsis>
///
/// <example>
/// Downsampling a spectral cube:
/// <srcblock>
///   using namespace casacore_mini;
///
///   ArrayLattice<float> full(IPosition{1024, 1024, 128}, 1.0f);
///
///   // Bin by 4 in each spatial axis, 2 in spectral.
///   RebinLattice<float> binned(full, IPosition{4, 4, 2});
///   assert(binned.shape() == (IPosition{256, 256, 64}));
///
///   // Read a single output pixel (mean of 4x4x2 = 32 input pixels).
///   float v = binned.get_at(IPosition{0, 0, 0});
/// </srcblock>
/// </example>
template <typename T>
class RebinLattice : public MaskedLattice<T> {
  public:
    /// Construct from a lattice and per-axis rebin factors.
    RebinLattice(const Lattice<T>& parent, IPosition factors);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Rebin factors per axis.
    [[nodiscard]] const IPosition& factors() const { return factors_; }

  private:
    const Lattice<T>* parent_;
    IPosition factors_;
    IPosition shape_;
};

// ── LatticeIterator<T> ────────────────────────────────────────────────

/// <summary>
/// Cursor-based iterator that walks through a lattice in tile-sized chunks.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// LatticeIterator<T> divides a lattice into rectangular tiles of a
/// requested cursor shape and iterates over them in Fortran order (the
/// first axis position advances fastest).  At each step the caller can:
///
/// <ul>
///   <li> Query the current tile position via <src>position()</src>.
///   <li> Obtain the actual tile shape (may be smaller at the lattice
///        boundary) via <src>cursor_shape()</src>.
///   <li> Read the current tile into a LatticeArray via <src>cursor()</src>.
///   <li> Write modified data back (for writable lattices) via
///        <src>write_cursor()</src>.
/// </ul>
///
/// Iteration terminates when <src>at_end()</src> returns true.  Call
/// <src>reset()</src> to restart from the beginning.
///
/// Two constructors are provided mirroring <src>SubLattice</src>:
/// <ul>
///   <li> Mutable: <src>LatticeIterator(Lattice<T>&, cursor_shape)</src>.
///   <li> Read-only: <src>LatticeIterator(const Lattice<T>&, cursor_shape)</src>.
/// </ul>
///
/// Tile-based iteration is the recommended approach for processing
/// data that may not fit entirely in memory (e.g. <src>PagedArray<T></src>
/// or <src>TempLattice<T></src> with disk backing).
/// </synopsis>
///
/// <example>
/// Iterating over a lattice in 64 x 64 x 1 tiles:
/// <srcblock>
///   using namespace casacore_mini;
///
///   ArrayLattice<float> lat(IPosition{256, 256, 16}, 1.0f);
///   LatticeIterator<float> it(lat, IPosition{64, 64, 1});
///
///   for (; !it.at_end(); ++it) {
///       auto tile = it.cursor();         // LatticeArray<float>
///       tile.make_unique();
///       // ... process tile in place ...
///       it.write_cursor(tile);           // write modified tile back
///   }
/// </srcblock>
/// </example>
///
/// <note role="caution">
/// LatticeIterator holds a raw pointer to its lattice; the lattice must
/// remain alive for the entire iteration.  Modifying the lattice shape
/// while an iterator is active leads to undefined behaviour.
/// </note>
template <typename T>
class LatticeIterator {
  public:
    /// Construct an iterator over the entire lattice with given cursor shape.
    LatticeIterator(Lattice<T>& lattice, IPosition cursor_shape);
    /// Construct a read-only iterator.
    LatticeIterator(const Lattice<T>& lattice, IPosition cursor_shape);

    /// Reset to the beginning.
    void reset();
    /// Advance to the next position.
    LatticeIterator& operator++();
    /// True if all positions have been visited.
    [[nodiscard]] bool at_end() const;

    /// Current cursor position (start index of the current chunk).
    [[nodiscard]] const IPosition& position() const { return position_; }
    /// Shape of the current cursor chunk (may be smaller at edges).
    [[nodiscard]] IPosition cursor_shape() const;
    /// Read-only access to the current chunk.
    [[nodiscard]] LatticeArray<T> cursor() const;
    /// Write updated data back to the current position (for writable lattices).
    void write_cursor(const LatticeArray<T>& data);

  private:
    Lattice<T>* lattice_rw_ = nullptr;
    const Lattice<T>* lattice_ = nullptr;
    IPosition requested_shape_;
    IPosition position_;
    bool writable_ = false;
};

// ── Template method implementations ───────────────────────────────────

template <typename T>
void Lattice<T>::set(const T& value) {
    auto data = LatticeArray<T>(shape(), value);
    put(data);
}

template <typename T>
void Lattice<T>::apply(const std::function<T(T)>& func) {
    auto data = get();
    data.make_unique();
    for (std::size_t i = 0; i < data.nelements(); ++i) {
        data.mutable_data()[i] = func(data[i]);
    }
    put(data);
}

// ── ArrayLattice<T> implementations ───────────────────────────────────

template <typename T>
ArrayLattice<T>::ArrayLattice(IPosition shape) : data_(std::move(shape)) {}

template <typename T>
ArrayLattice<T>::ArrayLattice(IPosition shape, T value)
    : data_(std::move(shape), value) {}

template <typename T>
ArrayLattice<T>::ArrayLattice(LatticeArray<T> data) : data_(std::move(data)) {}

template <typename T>
LatticeArray<bool> ArrayLattice<T>::get_mask() const {
    return LatticeArray<bool>(data_.shape(), true);
}

template <typename T>
LatticeArray<bool> ArrayLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> ArrayLattice<T>::get_slice(const Slicer& slicer) const {
    return data_.get_slice(slicer);
}

template <typename T>
T ArrayLattice<T>::get_at(const IPosition& where) const {
    return data_.at(where);
}

template <typename T>
void ArrayLattice<T>::put(const LatticeArray<T>& data) {
    data_ = data;
    data_.make_unique();
}

template <typename T>
void ArrayLattice<T>::put_slice(const LatticeArray<T>& data,
                                const Slicer& slicer) {
    data_.make_unique();
    data_.put_slice(data, slicer);
}

template <typename T>
void ArrayLattice<T>::put_at(const T& value, const IPosition& where) {
    data_.make_unique();
    data_.put(where, value);
}

// ── SubLattice<T> implementations ─────────────────────────────────────

template <typename T>
SubLattice<T>::SubLattice(Lattice<T>& parent, Slicer slicer)
    : parent_rw_(&parent),
      parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      writable_(parent.is_writable()) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
SubLattice<T>::SubLattice(const Lattice<T>& parent, Slicer slicer)
    : parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      writable_(false) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
LatticeArray<bool> SubLattice<T>::get_mask() const {
    return LatticeArray<bool>(sub_shape_, true);
}

template <typename T>
LatticeArray<bool> SubLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> SubLattice<T>::get() const {
    return parent_->get_slice(region_);
}

template <typename T>
LatticeArray<T> SubLattice<T>::get_slice(const Slicer& slicer) const {
    return parent_->get_slice(to_parent_slicer(slicer));
}

template <typename T>
T SubLattice<T>::get_at(const IPosition& where) const {
    return parent_->get_at(to_parent_index(where));
}

template <typename T>
void SubLattice<T>::put(const LatticeArray<T>& data) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_slice(data, region_);
}

template <typename T>
void SubLattice<T>::put_slice(const LatticeArray<T>& data,
                              const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_slice(data, to_parent_slicer(slicer));
}

template <typename T>
void SubLattice<T>::put_at(const T& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_at(value, to_parent_index(where));
}

template <typename T>
IPosition SubLattice<T>::to_parent_index(const IPosition& local) const {
    IPosition result(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        result[d] = region_.start()[d] + local[d] * region_.stride()[d];
    }
    return result;
}

template <typename T>
Slicer SubLattice<T>::to_parent_slicer(const Slicer& local) const {
    IPosition start(local.ndim());
    IPosition stride(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        start[d] = region_.start()[d] + local.start()[d] * region_.stride()[d];
        stride[d] = local.stride()[d] * region_.stride()[d];
    }
    return Slicer(std::move(start), IPosition(local.length()), std::move(stride));
}

// ── TempLattice<T> implementations ───────────────────────────────────

template <typename T>
TempLattice<T>::TempLattice(IPosition shape, std::size_t max_memory_bytes) {
    auto total_bytes = static_cast<std::size_t>(shape.product()) * sizeof(T);
    if (total_bytes <= max_memory_bytes) {
        impl_ = std::make_unique<ArrayLattice<T>>(std::move(shape));
    } else {
        // For large data, use a temporary file on disk.
        auto tmp_dir = std::filesystem::temp_directory_path() / "casacore_mini_tmp";
        std::filesystem::create_directories(tmp_dir);
        // Generate a unique name.
        auto name = "tmp_lattice_" + std::to_string(
            std::hash<std::string>{}(shape.to_string() +
                                     std::to_string(reinterpret_cast<std::uintptr_t>(this))));
        auto path = tmp_dir / name;
        impl_ = std::make_unique<PagedArray<T>>(std::move(shape), path);
    }
}

template <typename T>
LatticeArray<bool> TempLattice<T>::get_mask() const {
    return LatticeArray<bool>(impl_->shape(), true);
}

template <typename T>
LatticeArray<bool> TempLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

// ── LatticeIterator<T> implementations ────────────────────────────────

template <typename T>
LatticeIterator<T>::LatticeIterator(Lattice<T>& lattice, IPosition cursor_shape)
    : lattice_rw_(&lattice),
      lattice_(&lattice),
      requested_shape_(std::move(cursor_shape)),
      position_(lattice.shape().ndim(), 0),
      writable_(true) {}

template <typename T>
LatticeIterator<T>::LatticeIterator(const Lattice<T>& lattice,
                                    IPosition cursor_shape)
    : lattice_(&lattice),
      requested_shape_(std::move(cursor_shape)),
      position_(lattice.shape().ndim(), 0),
      writable_(false) {}

template <typename T>
void LatticeIterator<T>::reset() {
    for (std::size_t i = 0; i < position_.ndim(); ++i) position_[i] = 0;
}

template <typename T>
LatticeIterator<T>& LatticeIterator<T>::operator++() {
    const auto& shape = lattice_->shape();
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        position_[d] += requested_shape_[d];
        if (position_[d] < shape[d]) return *this;
        position_[d] = 0;
    }
    // All dimensions wrapped → set to end sentinel.
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        position_[d] = shape[d];
    }
    return *this;
}

template <typename T>
bool LatticeIterator<T>::at_end() const {
    const auto& shape = lattice_->shape();
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        if (position_[d] >= shape[d]) return true;
    }
    return false;
}

template <typename T>
IPosition LatticeIterator<T>::cursor_shape() const {
    const auto& shape = lattice_->shape();
    IPosition cs(shape.ndim());
    for (std::size_t d = 0; d < shape.ndim(); ++d) {
        auto remaining = shape[d] - position_[d];
        cs[d] = std::min(requested_shape_[d], remaining);
    }
    return cs;
}

template <typename T>
LatticeArray<T> LatticeIterator<T>::cursor() const {
    auto cs = cursor_shape();
    return lattice_->get_slice(Slicer(IPosition(position_), cs));
}

template <typename T>
void LatticeIterator<T>::write_cursor(const LatticeArray<T>& data) {
    if (!writable_) throw std::runtime_error("LatticeIterator is read-only");
    auto cs = cursor_shape();
    lattice_rw_->put_slice(data, Slicer(IPosition(position_), cs));
}

// ── LatticeConcat<T> implementations ──────────────────────────────────

template <typename T>
LatticeConcat<T>::LatticeConcat(std::vector<const Lattice<T>*> lattices,
                                std::size_t axis)
    : lattices_(std::move(lattices)), axis_(axis) {
    if (lattices_.empty()) {
        throw std::runtime_error("LatticeConcat: no lattices given");
    }
    const auto ndim = lattices_[0]->shape().ndim();
    if (axis_ >= ndim) {
        throw std::runtime_error("LatticeConcat: axis out of range");
    }
    // Compute shape: same as first lattice except on the concat axis.
    shape_ = IPosition(lattices_[0]->shape());
    offsets_.push_back(0);
    for (std::size_t i = 1; i < lattices_.size(); ++i) {
        const auto& s = lattices_[i]->shape();
        if (s.ndim() != ndim) {
            throw std::runtime_error("LatticeConcat: mismatched ndim");
        }
        for (std::size_t d = 0; d < ndim; ++d) {
            if (d != axis_ && s[d] != shape_[d]) {
                throw std::runtime_error(
                    "LatticeConcat: mismatched shape on non-concat axis");
            }
        }
        offsets_.push_back(shape_[axis_]);
        shape_[axis_] += s[axis_];
    }
}

template <typename T>
LatticeArray<bool> LatticeConcat<T>::get_mask() const {
    return LatticeArray<bool>(shape_, true);
}

template <typename T>
LatticeArray<bool> LatticeConcat<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
std::pair<std::size_t, std::int64_t>
LatticeConcat<T>::locate(std::int64_t global_pos) const {
    for (std::size_t i = 0; i < lattices_.size(); ++i) {
        auto extent = lattices_[i]->shape()[axis_];
        if (global_pos < offsets_[i] + extent) {
            return {i, global_pos - offsets_[i]};
        }
    }
    throw std::runtime_error("LatticeConcat: position out of range");
}

template <typename T>
LatticeArray<T> LatticeConcat<T>::get() const {
    LatticeArray<T> result(shape_);
    // Copy each component lattice into the output.
    for (std::size_t i = 0; i < lattices_.size(); ++i) {
        auto src = lattices_[i]->get();
        IPosition start(shape_.ndim(), 0);
        start[axis_] = offsets_[i];
        result.put_slice(src, Slicer(std::move(start), lattices_[i]->shape()));
    }
    return result;
}

template <typename T>
LatticeArray<T> LatticeConcat<T>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <typename T>
T LatticeConcat<T>::get_at(const IPosition& where) const {
    auto [idx, local_pos] = locate(where[axis_]);
    IPosition local(where);
    local[axis_] = local_pos;
    return lattices_[idx]->get_at(local);
}

template <typename T>
void LatticeConcat<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

template <typename T>
void LatticeConcat<T>::put_slice(const LatticeArray<T>& /*data*/,
                                  const Slicer& /*slicer*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

template <typename T>
void LatticeConcat<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

// ── RebinLattice<T> implementations ──────────────────────────────────

template <typename T>
RebinLattice<T>::RebinLattice(const Lattice<T>& parent, IPosition factors)
    : parent_(&parent), factors_(std::move(factors)) {
    const auto& ps = parent.shape();
    if (factors_.ndim() != ps.ndim()) {
        throw std::runtime_error("RebinLattice: factors ndim mismatch");
    }
    shape_ = IPosition(ps.ndim());
    for (std::size_t d = 0; d < ps.ndim(); ++d) {
        if (factors_[d] <= 0) {
            throw std::runtime_error("RebinLattice: factors must be positive");
        }
        shape_[d] = (ps[d] + factors_[d] - 1) / factors_[d];
    }
}

template <typename T>
LatticeArray<bool> RebinLattice<T>::get_mask() const {
    return LatticeArray<bool>(shape_, true);
}

template <typename T>
LatticeArray<bool> RebinLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> RebinLattice<T>::get() const {
    auto src = parent_->get();
    LatticeArray<T> result(shape_);
    const auto ndim = shape_.ndim();
    const auto& ps = parent_->shape();

    // Iterate over every output element.
    auto total = shape_.product();
    for (std::int64_t flat = 0; flat < total; ++flat) {
        // Decompose flat index into output coordinates.
        IPosition out_pos(ndim);
        std::int64_t rem = flat;
        for (std::size_t d = 0; d < ndim; ++d) {
            out_pos[d] = rem % shape_[d];
            rem /= shape_[d];
        }

        // Average the bin in the source.
        double sum = 0.0;
        std::int64_t count = 0;

        // Compute bin bounds.
        std::vector<std::int64_t> lo(ndim);
        std::vector<std::int64_t> hi(ndim);
        for (std::size_t d = 0; d < ndim; ++d) {
            lo[d] = out_pos[d] * factors_[d];
            hi[d] = std::min(lo[d] + factors_[d], ps[d]);
        }

        // Iterate through the bin (works for any dimensionality).
        std::int64_t bin_total = 1;
        for (std::size_t d = 0; d < ndim; ++d) {
            bin_total *= (hi[d] - lo[d]);
        }
        for (std::int64_t b = 0; b < bin_total; ++b) {
            IPosition src_pos(ndim);
            std::int64_t brem = b;
            for (std::size_t d = 0; d < ndim; ++d) {
                auto extent = hi[d] - lo[d];
                src_pos[d] = lo[d] + brem % extent;
                brem /= extent;
            }
            sum += static_cast<double>(src.at(src_pos));
            ++count;
        }

        const double denom = static_cast<double>(count);
        result.put(out_pos, count > 0 ? static_cast<T>(sum / denom) : T{});
    }
    return result;
}

template <typename T>
LatticeArray<T> RebinLattice<T>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <typename T>
T RebinLattice<T>::get_at(const IPosition& where) const {
    const auto ndim = shape_.ndim();
    const auto& ps = parent_->shape();

    double sum = 0.0;
    std::int64_t count = 0;

    std::vector<std::int64_t> lo(ndim);
    std::vector<std::int64_t> hi(ndim);
    for (std::size_t d = 0; d < ndim; ++d) {
        lo[d] = where[d] * factors_[d];
        hi[d] = std::min(lo[d] + factors_[d], ps[d]);
    }

    std::int64_t bin_total = 1;
    for (std::size_t d = 0; d < ndim; ++d) {
        bin_total *= (hi[d] - lo[d]);
    }
    for (std::int64_t b = 0; b < bin_total; ++b) {
        IPosition src_pos(ndim);
        std::int64_t brem = b;
        for (std::size_t d = 0; d < ndim; ++d) {
            auto extent = hi[d] - lo[d];
            src_pos[d] = lo[d] + brem % extent;
            brem /= extent;
        }
        sum += static_cast<double>(parent_->get_at(src_pos));
        ++count;
    }
    const double denom = static_cast<double>(count);
    return count > 0 ? static_cast<T>(sum / denom) : T{};
}

template <typename T>
void RebinLattice<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

template <typename T>
void RebinLattice<T>::put_slice(const LatticeArray<T>& /*data*/,
                                 const Slicer& /*slicer*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

template <typename T>
void RebinLattice<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

} // namespace casacore_mini
