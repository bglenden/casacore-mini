// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/lattice.hpp"
#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/unit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Image class hierarchy: ImageInterface, PagedImage, TempImage, SubImage.
///
/// An image is a lattice with associated coordinate system, units, and
/// metadata. Images are persisted as casacore tables with a "map" column
/// for pixel data and table keywords for coordinate/metadata.

// ── Forward declarations ───────────────────────────────────────────────

template <typename T> class ImageInterface;
template <typename T> class PagedImage;
template <typename T> class TempImage;
template <typename T> class SubImage;
template <typename T> class ImageExpr;
template <typename T> class ImageConcat;

// ── ImageInfo ──────────────────────────────────────────────────────────

/// <summary>
/// Image-level metadata: restoring beam, object name, and image type.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// ImageInfo bundles the small set of scalar metadata that every
/// astronomical image carries beyond its pixel array:
/// <ul>
///   <li> A restoring beam described by major axis, minor axis, and position
///        angle (all in radians).  A beam is considered absent when either
///        <src>beam_major_rad</src> or <src>beam_minor_rad</src> is zero.
///   <li> An optional object name string (e.g., "NGC 1234").
///   <li> An image type string (e.g., "Intensity", "Beam", "SpectralIndex").
/// </ul>
///
/// The struct is serialised to and from a <src>Record</src> so it can be
/// stored as a table keyword inside a <src>PagedImage</src> on disk.  The
/// keyword name used by <src>PagedImage</src> is <src>"imageinfo"</src>.
///
/// <note role="caution">
/// All beam angles are stored in radians.  When reading beam parameters
/// from FITS or AIPS-IO sources, remember to convert from degrees or
/// arcseconds before constructing an <src>ImageInfo</src>.
/// </note>
/// </synopsis>
///
/// <example>
/// Attach a restoring beam to an in-memory image:
/// <srcblock>
///   using namespace casacore_mini;
///   ImageInfo info;
///   info.beam_major_rad = 1.5e-5;   // ~3 arcsec in radians
///   info.beam_minor_rad = 1.0e-5;
///   info.beam_pa_rad    = 0.0;
///   info.object_name    = "NGC 1234";
///   info.image_type     = "Intensity";
///
///   TempImage<float> img(IPosition({256, 256}), cs);
///   img.set_image_info(info);
///   assert(img.image_info().has_beam());
/// </srcblock>
/// </example>
struct ImageInfo {
    /// Restoring beam major axis (radians). 0 = no beam.
    double beam_major_rad = 0.0;
    /// Restoring beam minor axis (radians).
    double beam_minor_rad = 0.0;
    /// Restoring beam position angle (radians).
    double beam_pa_rad = 0.0;
    /// Object name (e.g., "NGC 1234").
    std::string object_name;
    /// Image type string (e.g., "Intensity").
    std::string image_type;

    /// Whether a restoring beam is defined.
    [[nodiscard]] bool has_beam() const {
        return beam_major_rad > 0.0 && beam_minor_rad > 0.0;
    }

    /// Serialize to Record.
    [[nodiscard]] Record to_record() const;
    /// Deserialize from Record.
    [[nodiscard]] static ImageInfo from_record(const Record& rec);

    [[nodiscard]] bool operator==(const ImageInfo& other) const = default;
};

// ── ImageInterface<T> ─────────────────────────────────────────────────

/// <summary>
/// Abstract base class for all astronomical image types.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> MaskedLattice<T> — pixel data + boolean mask interface
///   <li> CoordinateSystem — world-coordinate description of each pixel axis
///   <li> ImageInfo — restoring beam and object metadata
/// </prerequisite>
///
/// <synopsis>
/// ImageInterface<T> extends MaskedLattice<T> with the additional
/// metadata required for astronomical images:
/// <ul>
///   <li> A CoordinateSystem describing each pixel axis in world coordinates
///   <li> A brightness unit string (e.g. "Jy/beam", "K")
///   <li> An ImageInfo record holding the restoring beam, object name, and type
///   <li> A miscellaneous keyword Record for user-defined metadata
/// </ul>
///
/// The hierarchy mirrors casacore's ImageInterface design.  Concrete
/// subclasses include PagedImage (disk), TempImage (memory),
/// SubImage (windowed view), ImageExpr (expression result),
/// and ImageConcat (axis-concatenated view).
///
/// Mask convention: a pixel mask value of <src>true</src> means the
/// pixel is valid; <src>false</src> means it is flagged.
/// </synopsis>
///
/// <example>
/// Open a FITS-derived CASA image and read its coordinate system:
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> img("my_image.image");
///   std::cout << "Shape: " << img.shape().to_string() << "\n";
///   std::cout << "Units: " << img.units() << "\n";
///   auto stats = image_utilities::statistics(img);
///   std::cout << "Peak: " << stats.max_val << "\n";
/// </srcblock>
/// </example>
///
/// <motivation>
/// A common abstract interface allows algorithms (statistics, regridding,
/// sub-imaging, expression evaluation) to operate on any image type without
/// knowing whether pixels live on disk, in memory, or are computed on the
/// fly from a lattice expression.
/// </motivation>
template <typename T>
class ImageInterface : public MaskedLattice<T> {
  public:
    /// Coordinate system describing the axes.
    [[nodiscard]] virtual const CoordinateSystem& coordinates() const = 0;
    /// Mutable coordinate access (for updating after creation).
    virtual void set_coordinates(CoordinateSystem cs) = 0;

    /// Brightness unit string (e.g., "Jy/beam").
    [[nodiscard]] virtual const std::string& units() const = 0;
    /// Set the brightness unit.
    virtual void set_units(std::string unit_str) = 0;

    /// Image-level metadata (beam, object name, type).
    [[nodiscard]] virtual const ImageInfo& image_info() const = 0;
    /// Mutable image info.
    virtual void set_image_info(ImageInfo info) = 0;

    /// Miscellaneous keywords (user-defined metadata).
    [[nodiscard]] virtual const Record& misc_info() const = 0;
    /// Set miscellaneous keywords.
    virtual void set_misc_info(Record info) = 0;

    /// Image name (typically filename/path).
    [[nodiscard]] virtual std::string name() const = 0;
    /// Whether this image is backed by persistent storage.
    [[nodiscard]] virtual bool is_persistent() const { return false; }
};

// ── PagedImage<T> ─────────────────────────────────────────────────────

/// <summary>
/// Disk-backed image stored as a casacore table directory.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInterface<T> — abstract image base class
///   <li> CoordinateSystem — axis description serialized as a table keyword
///   <li> Table — casacore on-disk table (read/write access)
/// </prerequisite>
///
/// <synopsis>
/// PagedImage stores pixel data on disk as a casacore table directory.
/// The table layout is:
/// <ul>
///   <li> One array column "map" backed by TiledShapeStMan
///   <li> Table keyword "coords"    → CoordinateSystem Record
///   <li> Table keyword "imageinfo" → ImageInfo Record
///   <li> Table keyword "miscinfo"  → miscellaneous Record
///   <li> Table keyword "units"     → brightness unit string
/// </ul>
/// This layout is binary-compatible with CASA images produced by
/// casacore's PagedImage class.
///
/// Pixel masks are stored in a mask subtable whose name is passed to
/// <src>make_mask()</src>.  The default mask is the one last registered
/// via <src>make_mask(..., set_default=true)</src>.
///
/// Metadata is loaded into memory on construction and flushed back to
/// the table either on <src>flush()</src> or on destruction.  Pixel data
/// is read and written directly through the table column without buffering.
///
/// <note role="caution">
/// Opening the same table path from two <src>PagedImage</src> instances
/// simultaneously (one writable) results in undefined behaviour.  Use
/// read-only opens when concurrent access is required.
/// </note>
/// </synopsis>
///
/// <example>
/// Create a new image on disk and write some pixels:
/// <srcblock>
///   using namespace casacore_mini;
///   CoordinateSystem cs = CoordinateSystem::make_simple_ra_dec(256, 256);
///   PagedImage<float> img(IPosition({256, 256}), cs, "my_image.image");
///   img.set_units("Jy/beam");
///
///   LatticeArray<float> pixels(IPosition({256, 256}), 0.0f);
///   pixels.put(IPosition({128, 128}), 1.0f);   // point source at centre
///   img.put(pixels);
///   img.flush();
/// </srcblock>
///
/// Re-open an existing image read-only:
/// <srcblock>
///   PagedImage<float> img("my_image.image");   // writable=false by default
///   auto stats = image_utilities::statistics(img);
///   std::cout << "Peak: " << stats.max_val << "\n";
/// </srcblock>
/// </example>
///
/// <motivation>
/// Provides a CASA-compatible on-disk image format so that images
/// produced by casacore-mini can be read by CASA and vice versa without
/// any conversion step.
/// </motivation>
template <typename T>
class PagedImage : public ImageInterface<T> {
  public:
    /// Create a new image on disk.
    PagedImage(IPosition shape, CoordinateSystem cs,
               const std::filesystem::path& path);
    /// Open an existing image from disk (read-only by default).
    explicit PagedImage(const std::filesystem::path& path,
                        bool writable = false);

    // -- Lattice interface --
    [[nodiscard]] const IPosition& shape() const override;
    [[nodiscard]] bool is_writable() const override;
    [[nodiscard]] bool is_paged() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    // -- Image interface --
    [[nodiscard]] const CoordinateSystem& coordinates() const override;
    void set_coordinates(CoordinateSystem cs) override;
    [[nodiscard]] const std::string& units() const override;
    void set_units(std::string unit_str) override;
    [[nodiscard]] const ImageInfo& image_info() const override;
    void set_image_info(ImageInfo info) override;
    [[nodiscard]] const Record& misc_info() const override;
    void set_misc_info(Record info) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_persistent() const override { return true; }

    /// Create a named pixel mask and optionally set it as the default.
    /// Returns after writing the mask data to the mask subtable.
    void make_mask(const std::string& mask_name,
                   const LatticeArray<bool>& mask_data,
                   bool set_default = true);

    /// Flush all metadata and pixel data to disk.
    void flush();

  private:
    void save_metadata();
    void load_metadata();

    struct Impl;
    std::shared_ptr<Impl> impl_;
};

// ── TempImage<T> ──────────────────────────────────────────────────────

/// <summary>
/// Temporary in-memory image with no disk persistence.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInterface<T> — abstract image base class
///   <li> LatticeArray<T> — in-memory pixel storage
/// </prerequisite>
///
/// <synopsis>
/// TempImage holds all pixel data in a <src>LatticeArray<T></src> residing
/// entirely in RAM.  It satisfies the full <src>ImageInterface<T></src>
/// contract — coordinate system, units, ImageInfo, and misc keywords — but
/// never touches the filesystem.
///
/// Because storage is in-memory, <src>is_persistent()</src> returns
/// <src>false</src> and <src>is_paged()</src> returns <src>false</src>.
/// The image is always writable.
///
/// An optional boolean pixel mask can be attached via <src>attach_mask()</src>.
/// Until a mask is attached, <src>has_pixel_mask()</src> returns
/// <src>false</src> and all pixels are treated as valid.
///
/// TempImage is useful as a scratch buffer for algorithmic intermediate
/// results (e.g., regridding output, convolution output) before writing
/// to a <src>PagedImage</src>.
/// </synopsis>
///
/// <example>
/// Build a temporary image, fill it, and copy it to disk:
/// <srcblock>
///   using namespace casacore_mini;
///   CoordinateSystem cs = CoordinateSystem::make_simple_ra_dec(64, 64);
///   TempImage<float> tmp(IPosition({64, 64}), cs);
///   tmp.set_units("Jy/beam");
///
///   LatticeArray<float> pixels(IPosition({64, 64}), 1.0f);
///   tmp.put(pixels);
///
///   PagedImage<float> disk(IPosition({64, 64}), cs, "output.image");
///   image_utilities::copy_image(tmp, disk);
///   disk.flush();
/// </srcblock>
/// </example>
template <typename T>
class TempImage : public ImageInterface<T> {
  public:
    /// Create a temporary image with given shape and coordinate system.
    TempImage(IPosition shape, CoordinateSystem cs);

    // -- Lattice interface --
    [[nodiscard]] const IPosition& shape() const override { return data_.shape(); }
    [[nodiscard]] bool is_writable() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override { return has_mask_; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override { return data_; }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override {
        return data_.get_slice(slicer);
    }
    [[nodiscard]] T get_at(const IPosition& where) const override {
        return data_.at(where);
    }

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    // -- Image interface --
    [[nodiscard]] const CoordinateSystem& coordinates() const override { return cs_; }
    void set_coordinates(CoordinateSystem cs) override { cs_ = std::move(cs); }
    [[nodiscard]] const std::string& units() const override { return units_; }
    void set_units(std::string unit_str) override { units_ = std::move(unit_str); }
    [[nodiscard]] const ImageInfo& image_info() const override { return info_; }
    void set_image_info(ImageInfo info) override { info_ = std::move(info); }
    [[nodiscard]] const Record& misc_info() const override { return misc_; }
    void set_misc_info(Record info) override { misc_ = std::move(info); }
    [[nodiscard]] std::string name() const override { return "TempImage"; }

    /// Attach a pixel mask.
    ///
    /// The mask array must have the same shape as the image.  Once attached,
    /// <src>has_pixel_mask()</src> returns <src>true</src>.  Calling
    /// <src>attach_mask()</src> a second time replaces the previous mask.
    void attach_mask(LatticeArray<bool> mask);

  private:
    LatticeArray<T> data_;
    LatticeArray<bool> mask_;
    bool has_mask_ = false;
    CoordinateSystem cs_;
    std::string units_;
    ImageInfo info_;
    Record misc_;
};

// ── SubImage<T> ───────────────────────────────────────────────────────

/// <summary>
/// A windowed, non-copying view into a sub-region of another image.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInterface<T> — abstract image base class
///   <li> Slicer — describes the sub-region (start, length, stride per axis)
///   <li> CoordinateSystem — adjusted to reflect reference-pixel shift
/// </prerequisite>
///
/// <synopsis>
/// SubImage presents a rectangular region of a parent image as if it were
/// an independent image.  No pixel data is copied on construction; all
/// read and write operations are forwarded to the parent through coordinate
/// translation.
///
/// The coordinate system stored in the SubImage is a copy of the parent's
/// system with the reference pixel shifted to account for the slicer's
/// start position and stride.  Units, ImageInfo, and miscellaneous keywords
/// are delegated to the parent at every access — there is no independent copy.
///
/// Writability mirrors the parent: a SubImage constructed from a mutable
/// <src>ImageInterface<T>&</src> is writable; one constructed from a
/// <src>const ImageInterface<T>&</src> is read-only.  Attempting any write
/// operation on a read-only SubImage throws <src>std::runtime_error</src>.
///
/// <note role="caution">
/// The parent image must outlive all SubImage instances that reference it.
/// SubImage stores a raw pointer to the parent and performs no lifetime
/// management.
/// </note>
/// </synopsis>
///
/// <example>
/// Extract the central quarter of a 256x256 image and compute statistics:
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> img("my_image.image");
///
///   Slicer region(IPosition({64, 64}),
///                 IPosition({192, 192}),
///                 Slicer::endIsLast);
///   SubImage<float> sub(img, region);
///
///   auto stats = image_utilities::statistics(sub);
///   std::cout << "Sub-region peak: " << stats.max_val << "\n";
/// </srcblock>
/// </example>
template <typename T>
class SubImage : public ImageInterface<T> {
  public:
    /// Construct from an image and a slicer (writable if parent is).
    SubImage(ImageInterface<T>& parent, Slicer slicer);
    /// Construct a read-only sub-image.
    SubImage(const ImageInterface<T>& parent, Slicer slicer);

    // -- Lattice interface --
    [[nodiscard]] const IPosition& shape() const override { return sub_shape_; }
    [[nodiscard]] bool is_writable() const override { return writable_; }
    [[nodiscard]] bool is_paged() const override { return parent_->is_paged(); }
    [[nodiscard]] bool has_pixel_mask() const override {
        return parent_->has_pixel_mask();
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    // -- Image interface --
    [[nodiscard]] const CoordinateSystem& coordinates() const override { return cs_; }
    void set_coordinates(CoordinateSystem cs) override { cs_ = std::move(cs); }
    [[nodiscard]] const std::string& units() const override { return parent_->units(); }
    void set_units(std::string unit_str) override;
    [[nodiscard]] const ImageInfo& image_info() const override {
        return parent_->image_info();
    }
    void set_image_info(ImageInfo info) override;
    [[nodiscard]] const Record& misc_info() const override {
        return parent_->misc_info();
    }
    void set_misc_info(Record info) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_persistent() const override {
        return parent_->is_persistent();
    }

  private:
    IPosition to_parent_index(const IPosition& local) const;
    Slicer to_parent_slicer(const Slicer& local) const;

    ImageInterface<T>* parent_rw_ = nullptr;
    const ImageInterface<T>* parent_ = nullptr;
    Slicer region_;
    IPosition sub_shape_;
    CoordinateSystem cs_;
    bool writable_ = false;
};

// ── ImageBeamSet ──────────────────────────────────────────────────────

/// <summary>
/// Per-channel (and optionally per-Stokes) restoring beam table.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInfo — single restoring beam for non-spectral images
/// </prerequisite>
///
/// <synopsis>
/// ImageBeamSet generalises the single restoring beam in <src>ImageInfo</src>
/// to a two-dimensional grid indexed by [channel][Stokes].  This is needed
/// for spectral cubes produced by multi-scale or multi-frequency synthesis
/// deconvolution where the synthesised beam shape varies with frequency.
///
/// The storage is a <src>std::vector<std::vector<Beam>></src> where the outer
/// index is channel and the inner index is Stokes.  A single-beam set is
/// represented as a 1x1 grid and can be tested with <src>is_single()</src>.
///
/// Serialisation to and from a <src>Record</src> is provided for storage as
/// a table keyword inside a <src>PagedImage</src>.
///
/// <note role="caution">
/// All beam parameters (major, minor, position angle) are stored in radians
/// to match <src>ImageInfo</src> conventions.
/// </note>
/// </synopsis>
///
/// <example>
/// Build a per-channel beam set for a 128-channel cube:
/// <srcblock>
///   using namespace casacore_mini;
///   ImageBeamSet beams(128, 1);
///   for (std::size_t ch = 0; ch < 128; ++ch) {
///       double scale = 1.0 + 0.001 * static_cast<double>(ch);
///       beams.set(ch, 0, {1.5e-5 * scale, 1.0e-5 * scale, 0.0});
///   }
///   bool single = beams.is_single();  // false
/// </srcblock>
/// </example>
struct ImageBeamSet {
    /// A single beam triple (radians).
    struct Beam {
        double major = 0.0;
        double minor = 0.0;
        double pa = 0.0;
        [[nodiscard]] bool operator==(const Beam&) const = default;
    };

    /// Beams indexed [channel][stokes]. If single-beam, shape is (1,1).
    std::vector<std::vector<Beam>> beams;

    /// Construct an empty beam set.
    ImageBeamSet() = default;
    /// Construct a single-beam set.
    explicit ImageBeamSet(Beam single);
    /// Construct from nchan x nstokes grid (initialized to zero beams).
    ImageBeamSet(std::size_t nchan, std::size_t nstokes);

    /// Number of channels.
    [[nodiscard]] std::size_t nchan() const;
    /// Number of Stokes.
    [[nodiscard]] std::size_t nstokes() const;
    /// Whether this set contains a single beam.
    [[nodiscard]] bool is_single() const;

    /// Get beam at (channel, stokes).
    [[nodiscard]] const Beam& get(std::size_t chan, std::size_t stokes = 0) const;
    /// Set beam at (channel, stokes).
    void set(std::size_t chan, std::size_t stokes, Beam beam);

    /// Serialize to Record.
    [[nodiscard]] Record to_record() const;
    /// Deserialize from Record.
    [[nodiscard]] static ImageBeamSet from_record(const Record& rec);
};

// ── MaskSpecifier ─────────────────────────────────────────────────────

/// <summary>
/// Selects which named pixel mask to apply when opening a PagedImage.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// In casacore, a <src>PagedImage</src> can store multiple named boolean
/// masks as subtables.  MaskSpecifier encapsulates the choice of which mask
/// to activate.  An empty name (<src>""</src>) selects the image's current
/// default mask, which is determined by the <src>"logtable"</src> keyword
/// inside the table.
///
/// Passing a MaskSpecifier to image-opening functions allows callers to
/// override the default mask without modifying the on-disk image.
///
/// <note role="caution">
/// If the named mask does not exist in the image, the open will fail with
/// a descriptive error rather than silently falling back to the default.
/// </note>
/// </synopsis>
///
/// <example>
/// Open an image using a specific mask named "pbmask":
/// <srcblock>
///   using namespace casacore_mini;
///   MaskSpecifier ms("pbmask");
///   // Pass ms to the image-opening routine (implementation-dependent).
///   assert(!ms.use_default());
///   assert(ms.name == "pbmask");
/// </srcblock>
/// </example>
struct MaskSpecifier {
    /// Mask name ("" = default mask).
    std::string name;
    /// Whether to use the default mask.
    [[nodiscard]] bool use_default() const { return name.empty(); }

    MaskSpecifier() = default;
    explicit MaskSpecifier(std::string mask_name)
        : name(std::move(mask_name)) {}
};

// ── ImageExpr<T> ──────────────────────────────────────────────────────

/// <summary>
/// A read-only image formed by evaluating a lattice expression.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInterface<T> — abstract image base class
///   <li> LatticeArray<T> — in-memory storage for evaluated pixel data
///   <li> CoordinateSystem — inherited from the reference image
/// </prerequisite>
///
/// <synopsis>
/// ImageExpr wraps a pre-evaluated <src>LatticeArray<T></src> — the result
/// of computing a lattice arithmetic expression — as a fully-fledged
/// <src>ImageInterface<T></src>.  The coordinate system, units, and
/// ImageInfo are supplied at construction time (typically copied from a
/// reference image used in the expression).
///
/// Because the pixel data represents a computed result rather than editable
/// storage, all write operations (<src>put()</src>, <src>put_slice()</src>,
/// <src>put_at()</src>) throw <src>std::runtime_error</src>.
/// <src>is_writable()</src> always returns <src>false</src>.
///
/// The optional <src>expr_text</src> string stores the human-readable
/// expression (e.g., <src>"2.0 * img1 - img2"</src>) for informational
/// purposes and is returned by <src>name()</src> as
/// <src>"ImageExpr(expr_text)"</src>.
///
/// All pixels are treated as valid — <src>has_pixel_mask()</src> returns
/// <src>false</src> and <src>get_mask()</src> returns an all-<src>true</src>
/// array.
///
/// <note role="caution">
/// The expression is evaluated eagerly at construction.  For very large
/// images, consider chunking the evaluation to avoid peak memory usage equal
/// to twice the image size (input + result).
/// </note>
/// </synopsis>
///
/// <example>
/// Subtract a background image and inspect the result:
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> signal("signal.image");
///   PagedImage<float> bg("background.image");
///
///   auto sig_data = signal.get();
///   auto bg_data  = bg.get();
///   LatticeArray<float> diff_data(sig_data.shape());
///   auto n = sig_data.nelements();
///   for (std::size_t i = 0; i < n; ++i)
///       diff_data.mutable_data()[i] = sig_data[i] - bg_data[i];
///
///   ImageExpr<float> diff(std::move(diff_data),
///                         CoordinateSystem::restore(signal.coordinates().save()),
///                         "signal - background");
///   auto stats = image_utilities::statistics(diff);
///   std::cout << "Diff peak: " << stats.max_val << "\n";
/// </srcblock>
/// </example>
template <typename T>
class ImageExpr : public ImageInterface<T> {
  public:
    /// Construct from evaluated data and a reference coordinate system.
    ImageExpr(LatticeArray<T> data, CoordinateSystem cs,
              std::string expr_text = "");

    // -- Lattice interface --
    [[nodiscard]] const IPosition& shape() const override { return data_.shape(); }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;
    [[nodiscard]] LatticeArray<T> get() const override { return data_; }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override {
        return data_.get_slice(slicer);
    }
    [[nodiscard]] T get_at(const IPosition& where) const override {
        return data_.at(where);
    }
    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    // -- Image interface --
    [[nodiscard]] const CoordinateSystem& coordinates() const override { return cs_; }
    void set_coordinates(CoordinateSystem cs) override { cs_ = std::move(cs); }
    [[nodiscard]] const std::string& units() const override { return units_; }
    void set_units(std::string unit_str) override { units_ = std::move(unit_str); }
    [[nodiscard]] const ImageInfo& image_info() const override { return info_; }
    void set_image_info(ImageInfo info) override { info_ = std::move(info); }
    [[nodiscard]] const Record& misc_info() const override { return misc_; }
    void set_misc_info(Record info) override { misc_ = std::move(info); }
    [[nodiscard]] std::string name() const override;

    /// The expression text used to create this image.
    [[nodiscard]] const std::string& expression() const { return expr_text_; }

  private:
    LatticeArray<T> data_;
    CoordinateSystem cs_;
    std::string expr_text_;
    std::string units_;
    ImageInfo info_;
    Record misc_;
};

// ── ImageConcat<T> ────────────────────────────────────────────────────

/// <summary>
/// Read-only concatenation of multiple images along a single axis.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> ImageInterface<T> — abstract image base class
///   <li> LatticeConcat<T> — underlying pixel concatenation mechanism
///   <li> CoordinateSystem — inherited from the first component image
/// </prerequisite>
///
/// <synopsis>
/// ImageConcat presents a set of images joined end-to-end along one pixel
/// axis as a single <src>ImageInterface<T></src>.  The underlying pixel data
/// is collected into a <src>LatticeConcat<T></src> eagerly at construction
/// by reading from each component image.
///
/// The coordinate system, units, and ImageInfo are copied from the first
/// component image.  The concatenation axis extent is the sum of the
/// corresponding extents of all component images; all other axes must agree
/// in length.
///
/// Because the concatenated array is a snapshot taken at construction time,
/// subsequent modifications to the component images are not reflected.
/// All write operations throw <src>std::runtime_error</src>;
/// <src>is_writable()</src> always returns <src>false</src>.
///
/// Masks are not propagated from component images — <src>has_pixel_mask()</src>
/// returns <src>false</src> and all pixels are treated as valid.  If mask
/// propagation is required, collect and concatenate masks manually after
/// construction.
///
/// <note role="caution">
/// Component image pointers must remain valid for the lifetime of the
/// ImageConcat object.  ImageConcat does not take ownership of the
/// component images.
/// </note>
/// </synopsis>
///
/// <example>
/// Concatenate three spectral-window images along the frequency axis (axis 2):
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> spw0("spw0.image");
///   PagedImage<float> spw1("spw1.image");
///   PagedImage<float> spw2("spw2.image");
///
///   ImageConcat<float> cube(
///       {&spw0, &spw1, &spw2},
///       /*axis=*/2);
///
///   std::cout << "Combined shape: " << cube.shape().to_string() << "\n";
///   std::cout << "N images: " << cube.nimages() << "\n";
///
///   auto stats = image_utilities::statistics(cube);
///   std::cout << "Cube peak: " << stats.max_val << "\n";
/// </srcblock>
/// </example>
template <typename T>
class ImageConcat : public ImageInterface<T> {
  public:
    /// Construct from a list of images and the axis to concatenate on.
    ImageConcat(std::vector<const ImageInterface<T>*> images,
                std::size_t axis);

    // -- Lattice interface --
    [[nodiscard]] const IPosition& shape() const override { return concat_.shape(); }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;
    [[nodiscard]] LatticeArray<T> get() const override { return concat_.get(); }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override {
        return concat_.get_slice(slicer);
    }
    [[nodiscard]] T get_at(const IPosition& where) const override {
        return concat_.get_at(where);
    }
    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    // -- Image interface --
    [[nodiscard]] const CoordinateSystem& coordinates() const override { return cs_; }
    void set_coordinates(CoordinateSystem cs) override { cs_ = std::move(cs); }
    [[nodiscard]] const std::string& units() const override { return units_; }
    void set_units(std::string unit_str) override { units_ = std::move(unit_str); }
    [[nodiscard]] const ImageInfo& image_info() const override { return info_; }
    void set_image_info(ImageInfo info) override { info_ = std::move(info); }
    [[nodiscard]] const Record& misc_info() const override { return misc_; }
    void set_misc_info(Record info) override { misc_ = std::move(info); }
    [[nodiscard]] std::string name() const override;

    /// Number of component images.
    [[nodiscard]] std::size_t nimages() const { return images_.size(); }

  private:
    static LatticeConcat<T> build_concat(
        const std::vector<const ImageInterface<T>*>& images,
        std::size_t axis);

    std::vector<const ImageInterface<T>*> images_;
    LatticeConcat<T> concat_;
    CoordinateSystem cs_;
    std::string units_;
    ImageInfo info_;
    Record misc_;
};

// ── ImageSummary ──────────────────────────────────────────────────────

/// <summary>
/// Utility that prints a human-readable summary of an image's properties.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// ImageSummary is a stateless utility struct with a single static method
/// <src>print()</src>.  It reports the image name, shape, brightness units,
/// write status, and — when present — restoring beam parameters and object
/// name.
///
/// Output is written to any <src>std::ostream</src> (defaulting to
/// <src>std::cout</src>), making it easy to redirect to a log file or
/// string stream.
/// </synopsis>
///
/// <example>
/// Print a summary of a disk image to standard output:
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> img("my_image.image");
///   ImageSummary::print(img);
/// </srcblock>
///
/// Capture the summary as a string:
/// <srcblock>
///   std::ostringstream oss;
///   ImageSummary::print(img, oss);
///   std::string summary = oss.str();
/// </srcblock>
/// </example>
struct ImageSummary {
    /// Print a summary of the image to the given stream.
    template <typename T>
    static void print(const ImageInterface<T>& img,
                      std::ostream& os = std::cout);
};

// ── Utility functions ─────────────────────────────────────────────────

/// <summary>
/// Collection of free functions for common image operations.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// The <src>image_utilities</src> namespace gathers convenience functions
/// that operate on any <src>ImageInterface<T></src>:
/// <ul>
///   <li> <src>statistics()</src> — computes min, max, mean, and RMS over
///        all pixels (unmasked or masked alike; mask is not currently
///        applied during statistics accumulation).
///   <li> <src>copy_image()</src> — copies pixel data and all metadata
///        (coordinates, units, ImageInfo, misc keywords) from a source
///        image to a destination image of the same shape.
///   <li> <src>scale_image()</src> — applies a linear transformation
///        <src>dst = src * scale + offset</src> pixel-by-pixel, writing
///        the result into a pre-allocated destination image.
/// </ul>
///
/// All functions are template functions operating on the image pixel type
/// <src>T</src>.  The caller is responsible for ensuring that source and
/// destination shapes are compatible.
/// </synopsis>
namespace image_utilities {

/// Compute basic statistics (min, max, mean, rms) of an image.
template <typename T>
struct ImageStats {
    T min_val{};
    T max_val{};
    double mean = 0.0;
    double rms = 0.0;
    std::int64_t npixels = 0;
};

/// Compute statistics over the entire image.
///
/// All pixels are included regardless of mask status.  Returns a
/// zero-initialized <src>ImageStats</src> when the image is empty.
template <typename T>
ImageStats<T> statistics(const ImageInterface<T>& img);

/// Copy all pixels and metadata from src to dst.
///
/// Copies the pixel array, coordinate system, units, ImageInfo, and
/// miscellaneous keywords.  The destination image must already exist and
/// have the same shape as the source.
template <typename T>
void copy_image(const ImageInterface<T>& src, ImageInterface<T>& dst);

/// Convert pixel values by a scale factor and offset: dst = src * scale + offset.
///
/// The destination image must already exist and have the same shape as the
/// source.  The computation is performed in <src>double</src> precision and
/// the result is cast back to <src>T</src>.
template <typename T>
void scale_image(const ImageInterface<T>& src, ImageInterface<T>& dst,
                 double scale, double offset = 0.0);

} // namespace image_utilities

/// <summary>
/// Simple nearest-neighbor image reprojection onto a new coordinate grid.
/// </summary>
///
/// <synopsis>
/// <src>image_regrid()</src> reprojects the source image <src>src</src>
/// onto the coordinate grid defined by the destination image <src>dst</src>
/// using nearest-neighbor interpolation.
///
/// The algorithm iterates over every output pixel position, converts it to
/// world coordinates via <src>dst</src>'s coordinate system, then maps the
/// world coordinate back to a pixel position in <src>src</src>'s coordinate
/// system.  The source pixel at the rounded (nearest-neighbor) position is
/// copied to the output.  Output pixels whose corresponding source position
/// falls outside the source image bounds are left at their default-initialized
/// value (zero for arithmetic types).
///
/// This is a simplified analogue of casacore's <src>ImageRegrid</src> class.
/// It handles axis permutations and grid changes that can be expressed through
/// the coordinate systems of <src>src</src> and <src>dst</src>, but does not
/// perform:
/// <ul>
///   <li> Higher-order interpolation (bilinear, bicubic, Lanczos).
///   <li> Spectral or polarization axis regridding beyond coordinate mapping.
///   <li> Flux conservation (division by Jacobian of the coordinate transform).
/// </ul>
///
/// The destination image must be pre-allocated with the desired output shape
/// and coordinate system before calling this function.
///
/// <note role="caution">
/// Nearest-neighbor interpolation introduces aliasing artefacts when
/// up-sampling by large factors.  For publication-quality imaging, use
/// a higher-order interpolation scheme.
/// </note>
/// </synopsis>
///
/// <example>
/// Regrid a 64x64 image onto a 128x128 grid:
/// <srcblock>
///   using namespace casacore_mini;
///   PagedImage<float> src("low_res.image");
///
///   CoordinateSystem hi_cs = make_hi_res_cs(128, 128); // user-defined
///   TempImage<float>  dst(IPosition({128, 128}), hi_cs);
///
///   image_regrid(src, dst);
///
///   PagedImage<float> out(IPosition({128, 128}), hi_cs, "hi_res.image");
///   image_utilities::copy_image(dst, out);
///   out.flush();
/// </srcblock>
/// </example>
template <typename T>
void image_regrid(const ImageInterface<T>& src, ImageInterface<T>& dst);

// ═══════════════════════════════════════════════════════════════════════
// Template implementations
// ═══════════════════════════════════════════════════════════════════════

// ── TempImage<T> ──────────────────────────────────────────────────────

template <typename T>
TempImage<T>::TempImage(IPosition shape, CoordinateSystem cs)
    : data_(std::move(shape)), cs_(std::move(cs)) {}

template <typename T>
LatticeArray<bool> TempImage<T>::get_mask() const {
    if (has_mask_) return mask_;
    return LatticeArray<bool>(data_.shape(), true);
}

template <typename T>
LatticeArray<bool> TempImage<T>::get_mask_slice(
    const Slicer& slicer) const {
    if (has_mask_) return mask_.get_slice(slicer);
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
void TempImage<T>::put(const LatticeArray<T>& data) {
    data_ = data;
    data_.make_unique();
}

template <typename T>
void TempImage<T>::put_slice(const LatticeArray<T>& data,
                             const Slicer& slicer) {
    data_.make_unique();
    data_.put_slice(data, slicer);
}

template <typename T>
void TempImage<T>::put_at(const T& value, const IPosition& where) {
    data_.make_unique();
    data_.put(where, value);
}

template <typename T>
void TempImage<T>::attach_mask(LatticeArray<bool> mask) {
    mask_ = std::move(mask);
    has_mask_ = true;
}

// ── SubImage<T> ───────────────────────────────────────────────────────

template <typename T>
SubImage<T>::SubImage(ImageInterface<T>& parent, Slicer slicer)
    : parent_rw_(&parent),
      parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      cs_(CoordinateSystem::restore(parent.coordinates().save())),
      writable_(parent.is_writable()) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
SubImage<T>::SubImage(const ImageInterface<T>& parent, Slicer slicer)
    : parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      cs_(CoordinateSystem::restore(parent.coordinates().save())),
      writable_(false) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
LatticeArray<bool> SubImage<T>::get_mask() const {
    if (!parent_->has_pixel_mask())
        return LatticeArray<bool>(sub_shape_, true);
    return parent_->get_mask_slice(region_);
}

template <typename T>
LatticeArray<bool> SubImage<T>::get_mask_slice(
    const Slicer& slicer) const {
    if (!parent_->has_pixel_mask())
        return LatticeArray<bool>(IPosition(slicer.length()), true);
    return parent_->get_mask_slice(to_parent_slicer(slicer));
}

template <typename T>
LatticeArray<T> SubImage<T>::get() const {
    return parent_->get_slice(region_);
}

template <typename T>
LatticeArray<T> SubImage<T>::get_slice(const Slicer& slicer) const {
    return parent_->get_slice(to_parent_slicer(slicer));
}

template <typename T>
T SubImage<T>::get_at(const IPosition& where) const {
    return parent_->get_at(to_parent_index(where));
}

template <typename T>
void SubImage<T>::put(const LatticeArray<T>& data) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->put_slice(data, region_);
}

template <typename T>
void SubImage<T>::put_slice(const LatticeArray<T>& data,
                            const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->put_slice(data, to_parent_slicer(slicer));
}

template <typename T>
void SubImage<T>::put_at(const T& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->put_at(value, to_parent_index(where));
}

template <typename T>
void SubImage<T>::set_units(std::string unit_str) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->set_units(std::move(unit_str));
}

template <typename T>
void SubImage<T>::set_image_info(ImageInfo info) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->set_image_info(std::move(info));
}

template <typename T>
void SubImage<T>::set_misc_info(Record info) {
    if (!writable_) throw std::runtime_error("SubImage is read-only");
    parent_rw_->set_misc_info(std::move(info));
}

template <typename T>
std::string SubImage<T>::name() const {
    return parent_->name() + " (sub)";
}

template <typename T>
IPosition SubImage<T>::to_parent_index(const IPosition& local) const {
    IPosition result(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        result[d] = region_.start()[d] + local[d] * region_.stride()[d];
    }
    return result;
}

template <typename T>
Slicer SubImage<T>::to_parent_slicer(const Slicer& local) const {
    IPosition start(local.ndim());
    IPosition stride(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        start[d] = region_.start()[d] + local.start()[d] * region_.stride()[d];
        stride[d] = local.stride()[d] * region_.stride()[d];
    }
    return Slicer(std::move(start), IPosition(local.length()), std::move(stride));
}

// ── ImageExpr<T> implementations ──────────────────────────────────────

template <typename T>
ImageExpr<T>::ImageExpr(LatticeArray<T> data, CoordinateSystem cs,
                        std::string expr_text)
    : data_(std::move(data)),
      cs_(std::move(cs)),
      expr_text_(std::move(expr_text)) {}

template <typename T>
LatticeArray<bool> ImageExpr<T>::get_mask() const {
    return LatticeArray<bool>(data_.shape(), true);
}

template <typename T>
LatticeArray<bool> ImageExpr<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
void ImageExpr<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("ImageExpr is read-only");
}

template <typename T>
void ImageExpr<T>::put_slice(const LatticeArray<T>& /*data*/,
                              const Slicer& /*slicer*/) {
    throw std::runtime_error("ImageExpr is read-only");
}

template <typename T>
void ImageExpr<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("ImageExpr is read-only");
}

template <typename T>
std::string ImageExpr<T>::name() const {
    return "ImageExpr(" + expr_text_ + ")";
}

// ── ImageConcat<T> implementations ────────────────────────────────────

template <typename T>
LatticeConcat<T> ImageConcat<T>::build_concat(
    const std::vector<const ImageInterface<T>*>& images,
    std::size_t axis) {
    std::vector<const Lattice<T>*> lats;
    lats.reserve(images.size());
    for (auto* img : images) lats.push_back(img);
    return LatticeConcat<T>(std::move(lats), axis);
}

template <typename T>
ImageConcat<T>::ImageConcat(std::vector<const ImageInterface<T>*> images,
                            std::size_t axis)
    : images_(std::move(images)),
      concat_(build_concat(images_, axis)) {
    if (!images_.empty()) {
        cs_ = CoordinateSystem::restore(images_[0]->coordinates().save());
        units_ = images_[0]->units();
        info_ = images_[0]->image_info();
    }
}

template <typename T>
LatticeArray<bool> ImageConcat<T>::get_mask() const {
    return LatticeArray<bool>(concat_.shape(), true);
}

template <typename T>
LatticeArray<bool> ImageConcat<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
void ImageConcat<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("ImageConcat is read-only");
}

template <typename T>
void ImageConcat<T>::put_slice(const LatticeArray<T>& /*data*/,
                                const Slicer& /*slicer*/) {
    throw std::runtime_error("ImageConcat is read-only");
}

template <typename T>
void ImageConcat<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("ImageConcat is read-only");
}

template <typename T>
std::string ImageConcat<T>::name() const {
    return "ImageConcat";
}

// ── ImageSummary implementation ───────────────────────────────────────

template <typename T>
void ImageSummary::print(const ImageInterface<T>& img, std::ostream& os) {
    os << "Image: " << img.name() << "\n";
    os << "Shape: " << img.shape().to_string() << "\n";
    os << "Units: " << img.units() << "\n";
    os << "Writable: " << (img.is_writable() ? "yes" : "no") << "\n";
    const auto& info = img.image_info();
    if (info.has_beam()) {
        os << "Beam: " << info.beam_major_rad << " x "
           << info.beam_minor_rad << " rad, PA "
           << info.beam_pa_rad << " rad\n";
    }
    if (!info.object_name.empty()) {
        os << "Object: " << info.object_name << "\n";
    }
}

// ── image_utilities implementations ───────────────────────────────────

namespace image_utilities {

template <typename T>
ImageStats<T> statistics(const ImageInterface<T>& img) {
    auto data = img.get();
    ImageStats<T> stats;
    auto n = data.nelements();
    stats.npixels = static_cast<std::int64_t>(n);
    if (n == 0) return stats;

    stats.min_val = data[0];
    stats.max_val = data[0];
    double sum = 0.0;
    double sum_sq = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        auto v = data[i];
        if (v < stats.min_val) stats.min_val = v;
        if (v > stats.max_val) stats.max_val = v;
        auto dv = static_cast<double>(v);
        sum += dv;
        sum_sq += dv * dv;
    }
    stats.mean = sum / static_cast<double>(n);
    stats.rms = std::sqrt(sum_sq / static_cast<double>(n));
    return stats;
}

template <typename T>
void copy_image(const ImageInterface<T>& src, ImageInterface<T>& dst) {
    dst.put(src.get());
    dst.set_coordinates(
        CoordinateSystem::restore(src.coordinates().save()));
    dst.set_units(src.units());
    dst.set_image_info(src.image_info());
    dst.set_misc_info(src.misc_info());
}

template <typename T>
void scale_image(const ImageInterface<T>& src, ImageInterface<T>& dst,
                 double scale, double offset) {
    auto data = src.get();
    LatticeArray<T> result(data.shape());
    auto n = data.nelements();
    for (std::size_t i = 0; i < n; ++i) {
        result.mutable_data()[i] =
            static_cast<T>(static_cast<double>(data[i]) * scale + offset);
    }
    dst.put(result);
}

} // namespace image_utilities

// ── image_regrid implementation ───────────────────────────────────────

template <typename T>
void image_regrid(const ImageInterface<T>& src, ImageInterface<T>& dst) {
    const auto& src_cs = src.coordinates();
    const auto& dst_cs = dst.coordinates();
    const auto& dst_shape = dst.shape();
    const auto& src_shape = src.shape();
    const auto ndim = dst_shape.ndim();

    auto src_data = src.get();
    LatticeArray<T> result(dst_shape);

    // For each output pixel, map to source via coordinate systems.
    auto total = dst_shape.product();
    for (std::int64_t flat = 0; flat < total; ++flat) {
        IPosition dst_pos(ndim);
        std::int64_t rem = flat;
        for (std::size_t d = 0; d < ndim; ++d) {
            dst_pos[d] = rem % dst_shape[d];
            rem /= dst_shape[d];
        }

        // Convert dst pixel → world → src pixel (nearest-neighbor).
        std::vector<double> pixel(ndim);
        for (std::size_t d = 0; d < ndim; ++d) {
            pixel[d] = static_cast<double>(dst_pos[d]);
        }
        auto world = dst_cs.to_world(pixel);
        auto src_pixel = src_cs.to_pixel(world);

        // Round to nearest and check bounds.
        IPosition src_pos(ndim);
        bool in_bounds = true;
        for (std::size_t d = 0; d < ndim; ++d) {
            auto idx = static_cast<std::int64_t>(std::round(src_pixel[d]));
            if (idx < 0 || idx >= src_shape[d]) {
                in_bounds = false;
                break;
            }
            src_pos[d] = idx;
        }

        if (in_bounds) {
            result.put(dst_pos, src_data.at(src_pos));
        }
        // else: leave as zero (default-initialized)
    }
    dst.put(result);
}

} // namespace casacore_mini
