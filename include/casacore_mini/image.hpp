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

/// Image-level metadata: restoring beam, object name, image type.
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

/// Abstract base class for all image types.
///
/// Extends `MaskedLattice<T>` with coordinate system, units, and metadata.
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

/// Disk-backed image stored as a casacore table.
///
/// Table layout:
/// - One array column "map" (pixel data, via TiledShapeStMan)
/// - Table keyword "coords" → CoordinateSystem Record
/// - Table keyword "imageinfo" → ImageInfo Record
/// - Table keyword "miscinfo" → misc Record
/// - Table keyword "units" → brightness unit string
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

/// Temporary in-memory image (not persisted to disk).
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

/// A view into a sub-region of another image.
///
/// Pixel data is forwarded to the parent. Coordinate system is adjusted
/// to reflect the sub-region's reference pixel shift.
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

/// Per-channel (and optionally per-Stokes) restoring beams.
///
/// Extends single-beam ImageInfo for spectral cubes where each channel
/// has its own beam. Provides storage and serialization.
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

/// Specifies which pixel mask to use when opening an image.
///
/// In casacore, images can have multiple named masks stored as table
/// columns. The default mask (empty name) uses the image's default mask.
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

/// An image formed by evaluating a lattice expression.
///
/// Wraps a `LatticeArray<T>` (result of expression evaluation) as an
/// `ImageInterface<T>`. The coordinate system is inherited from a
/// reference image. Pixel data is read-only (computed).
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

/// Read-only concatenation of multiple images along one axis.
///
/// Pixel data is eagerly read from the component images. The coordinate
/// system is derived from the first image with the concat axis extended.
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

/// Utility to print a summary of an image's properties.
struct ImageSummary {
    /// Print a summary of the image to the given stream.
    template <typename T>
    static void print(const ImageInterface<T>& img,
                      std::ostream& os = std::cout);
};

// ── Utility functions ─────────────────────────────────────────────────

/// Collection of image utility functions.
///
/// Provides helpers for common image operations: statistics, copying,
/// pixel arithmetic, and brightness unit conversion.
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
template <typename T>
ImageStats<T> statistics(const ImageInterface<T>& img);

/// Copy all pixels and metadata from src to dst.
template <typename T>
void copy_image(const ImageInterface<T>& src, ImageInterface<T>& dst);

/// Convert pixel values by a scale factor and offset: dst = src * scale + offset.
template <typename T>
void scale_image(const ImageInterface<T>& src, ImageInterface<T>& dst,
                 double scale, double offset = 0.0);

} // namespace image_utilities

/// Simple nearest-neighbor image regridding.
///
/// Reprojects an image onto a new coordinate grid by mapping each output
/// pixel to the nearest input pixel. This is a simplified version of
/// casacore's `ImageRegrid` — it handles coordinate mapping for grid
/// changes but does not perform interpolation beyond nearest-neighbor.
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
