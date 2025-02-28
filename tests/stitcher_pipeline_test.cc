// SPDX-FileCopyrightText: 2023 Tomas Krupka
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xpano/pipeline/stitcher_pipeline.h"

#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#ifdef XPANO_WITH_EXIV2
#include <exiv2/exiv2.hpp>
#endif
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "tests/utils.h"

using Catch::Matchers::Equals;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

const std::vector<std::filesystem::path> kInputs = {
    "data/image00.jpg", "data/image01.jpg", "data/image02.jpg",
    "data/image03.jpg", "data/image04.jpg", "data/image05.jpg",
    "data/image06.jpg", "data/image07.jpg", "data/image08.jpg",
    "data/image09.jpg",
};

int CountNonZero(const cv::Mat& image) {
  cv::Mat image_gray;
  cv::cvtColor(image, image_gray, cv::COLOR_BGR2GRAY);
  return cv::countNonZero(image_gray);
}

TEST_CASE("Stitcher pipeline defaults") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result = stitcher.RunLoading(kInputs, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  CHECK(result.images.size() == 10);
  CHECK(result.matches.size() == 17);
  REQUIRE(result.panos.size() == 2);
  REQUIRE_THAT(result.panos[0].ids, Equals<int>({1, 2, 3, 4, 5}));
  REQUIRE_THAT(result.panos[1].ids, Equals<int>({6, 7, 8}));

  const float eps = 0.02;

  // preview
  auto pano0 = stitcher.RunStitching(result, {.pano_id = 0}).get().pano;
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);
  REQUIRE(pano0.has_value());
  CHECK_THAT(pano0->rows, WithinRel(804, eps));
  CHECK_THAT(pano0->cols, WithinRel(2145, eps));

  // full resolution
  auto stitch_result =
      stitcher.RunStitching(result, {.pano_id = 1, .full_res = true}).get();
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);
  REQUIRE(stitch_result.pano.has_value());
  CHECK_THAT(stitch_result.pano->rows, WithinRel(1952, eps));
  CHECK_THAT(stitch_result.pano->cols, WithinRel(2651, eps));

  auto total_pixels = stitch_result.pano->rows * stitch_result.pano->cols;

  // auto fill
  REQUIRE(stitch_result.mask.has_value());
  auto inpaint_result =
      stitcher.RunInpainting(*stitch_result.pano, *stitch_result.mask, {})
          .get();
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  auto pano_pixels = CountNonZero(*stitch_result.pano);
  CHECK(total_pixels == inpaint_result.pixels_inpainted + pano_pixels);

  auto non_zero_pixels = CountNonZero(inpaint_result.pano);
  CHECK(total_pixels == non_zero_pixels);
}

// Clang-tidy doesn't like the macros
// NOLINTBEGIN(readability-function-cognitive-complexity)

TEST_CASE("Stitcher pipeline single pano matching") {
  xpano::pipeline::StitcherPipeline stitcher;
  auto result =
      stitcher
          .RunLoading(kInputs, {},
                      {.type = xpano::pipeline::MatchingType::kSinglePano})
          .get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  CHECK(result.images.size() == 10);
  CHECK(result.matches.empty());
  REQUIRE(result.panos.size() == 1);
  REQUIRE_THAT(result.panos[0].ids,
               Equals<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));

  for (const auto& image : result.images) {
    REQUIRE(image.GetKeypoints().empty());
    REQUIRE(image.GetDescriptors().empty());
  }
}

TEST_CASE("Stitcher pipeline no matching") {
  xpano::pipeline::StitcherPipeline stitcher;
  auto result = stitcher
                    .RunLoading(kInputs, {},
                                {.type = xpano::pipeline::MatchingType::kNone})
                    .get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  CHECK(result.images.size() == 10);
  CHECK(result.matches.empty());
  REQUIRE(result.panos.empty());

  for (const auto& image : result.images) {
    REQUIRE(image.GetKeypoints().empty());
    REQUIRE(image.GetDescriptors().empty());
  }
}

// NOLINTEND(readability-function-cognitive-complexity)

const std::vector<std::filesystem::path> kShuffledInputs = {
    "data/image01.jpg",  // Pano 1
    "data/image06.jpg",  // 2
    "data/image02.jpg",  // Pano 1
    "data/image07.jpg",  // 2
    "data/image03.jpg",  // Pano 1
    "data/image00.jpg",
    "data/image08.jpg",  // 2
    "data/image04.jpg",  // Pano 1
    "data/image09.jpg",
    "data/image05.jpg",  // Pano 1
};

TEST_CASE("Stitcher pipeline custom matching neighborhood") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result =
      stitcher.RunLoading(kShuffledInputs, {}, {.neighborhood_search_size = 3})
          .get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  CHECK(result.images.size() == 10);
  CHECK(result.matches.size() == 24);
  REQUIRE(result.panos.size() == 2);
  REQUIRE_THAT(result.panos[0].ids, Equals<int>({0, 2, 4, 7, 9}));
  REQUIRE_THAT(result.panos[1].ids, Equals<int>({1, 3, 6}));
}

// NOLINTBEGIN(readability-magic-numbers)

TEST_CASE("Stitcher pipeline larger neighborhood size") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result = stitcher
                    .RunLoading({"data/image01.jpg", "data/image02.jpg",
                                 "data/image03.jpg"},
                                {}, {.neighborhood_search_size = 10})
                    .get();

  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.size() == 3);
  REQUIRE(result.matches.size() == 3);  // [0 + 1], [0 + 2], [1 + 2]
}

// NOLINTEND(readability-magic-numbers)

TEST_CASE("Stitcher pipeline single image") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result = stitcher.RunLoading({"data/image01.jpg"}, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.size() == 1);
  REQUIRE(result.matches.empty());
}

TEST_CASE("Stitcher pipeline no images") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result = stitcher.RunLoading({}, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.empty());
  REQUIRE(result.matches.empty());
}

TEST_CASE("Stitcher pipeline loading options") {
  xpano::pipeline::StitcherPipeline stitcher;

  const int preview_size = 512;
  const int allowed_margin = 1.0;

  auto result = stitcher
                    .RunLoading({"data/image05.jpg", "data/image06.jpg"},
                                {.preview_longer_side = preview_size}, {})
                    .get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.size() == 2);

  auto landscape_full = result.images[0].GetFullRes();
  auto landscape_full_size = landscape_full.size();
  auto landscape_preview = result.images[0].GetPreview();
  auto landscape_preview_size = landscape_preview.size();

  CHECK(landscape_preview_size.width == preview_size);
  CHECK_THAT(landscape_preview_size.height,
             WithinAbs(preview_size / landscape_full_size.aspectRatio(),
                       allowed_margin));

  auto portrait_full = result.images[1].GetFullRes();
  auto portrait_full_size = portrait_full.size();
  auto portrait_preview = result.images[1].GetPreview();
  auto portrait_preview_size = portrait_preview.size();

  CHECK(portrait_preview_size.height == preview_size);
  CHECK_THAT(portrait_preview_size.width,
             WithinAbs(preview_size * portrait_full_size.aspectRatio(),
                       allowed_margin));
}

const std::vector<std::filesystem::path> kVerticalPanoInputs = {
    "data/image10.jpg",
    "data/image11.jpg",
    "data/image12.jpg",
};

TEST_CASE("Stitcher pipeline vertical pano") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result =
      stitcher
          .RunLoading(kVerticalPanoInputs, {}, {.neighborhood_search_size = 1})
          .get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  CHECK(result.images.size() == 3);
  CHECK(result.matches.size() == 2);
  REQUIRE(result.panos.size() == 1);
  REQUIRE_THAT(result.panos[0].ids, Equals<int>({0, 1, 2}));

  const float eps = 0.01;

  // preview
  auto pano0 = stitcher.RunStitching(result, {.pano_id = 0}).get().pano;
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);
  REQUIRE(pano0.has_value());
  CHECK_THAT(pano0->rows, WithinRel(1342, eps));
  CHECK_THAT(pano0->cols, WithinRel(1030, eps));
}

const std::vector<std::filesystem::path> kInputsWithExifMetadata = {
    "data/image06.jpg",
    "data/image07.jpg",
    "data/image08.jpg",
};

TEST_CASE("ExportWithMetadata") {
  const std::filesystem::path tmp_path =
      xpano::tests::TmpPath().replace_extension("jpg");

  xpano::pipeline::StitcherPipeline stitcher;
  auto data = stitcher.RunLoading(kInputsWithExifMetadata, {}, {}).get();
  REQUIRE(data.panos.size() == 1);
  stitcher.RunStitching(data, {.pano_id = 0, .export_path = tmp_path}).get();

  const float eps = 0.01;

  REQUIRE(std::filesystem::exists(tmp_path));
  auto image = cv::imread(tmp_path.string());
  REQUIRE(!image.empty());
  CHECK_THAT(image.rows, WithinRel(977, eps));
  CHECK_THAT(image.cols, WithinRel(1334, eps));

#ifdef XPANO_WITH_EXIV2
  auto read_img = Exiv2::ImageFactory::open(tmp_path.string());
  read_img->readMetadata();
  auto exif = read_img->exifData();

  auto software = exif["Exif.Image.Software"].toString();
  REQUIRE(software.starts_with("Xpano"));

  auto width = exif["Exif.Photo.PixelXDimension"].toUint32();
  auto height = exif["Exif.Photo.PixelYDimension"].toUint32();
  CHECK(width == image.cols);
  CHECK(height == image.rows);

  auto orientation = exif["Exif.Image.Orientation"].toUint32();
  CHECK(orientation == xpano::kExifDefaultOrientation);

  auto thumb = Exiv2::ExifThumbC(exif);
  CHECK_THAT(thumb.mimeType(), Equals(""));
  CHECK_THAT(thumb.extension(), Equals(""));
  CHECK(thumb.copy().empty());
#endif
  std::filesystem::remove(tmp_path);
}

#ifdef XPANO_WITH_EXIV2
bool TagExists(const Exiv2::ExifData& exif, const std::string& tag) {
  return exif.findKey(Exiv2::ExifKey(tag)) != exif.end();
}
#endif

TEST_CASE("ExportWithoutMetadata") {
  const std::filesystem::path tmp_path =
      xpano::tests::TmpPath().replace_extension("jpg");

  xpano::pipeline::StitcherPipeline stitcher;
  auto data = stitcher.RunLoading(kInputsWithExifMetadata, {}, {}).get();
  REQUIRE(data.panos.size() == 1);
  stitcher
      .RunStitching(data, {.pano_id = 0,
                           .export_path = tmp_path,
                           .metadata = {.copy_from_first_image = false}})
      .get();

  const float eps = 0.01;
  REQUIRE(std::filesystem::exists(tmp_path));

#ifdef XPANO_WITH_EXIV2
  auto read_img = Exiv2::ImageFactory::open(tmp_path.string());
  read_img->readMetadata();
  auto exif = read_img->exifData();

  auto software = exif["Exif.Image.Software"].toString();
  REQUIRE(software.starts_with("Xpano"));

  CHECK(!TagExists(exif, "Exif.Photo.PixelXDimension"));
  CHECK(!TagExists(exif, "Exif.Photo.PixelYDimension"));
  CHECK(!TagExists(exif, "Exif.Image.Orientation"));

  auto thumb = Exiv2::ExifThumbC(exif);
  CHECK_THAT(thumb.mimeType(), Equals(""));
  CHECK_THAT(thumb.extension(), Equals(""));
  CHECK(thumb.copy().empty());
#endif

  std::filesystem::remove(tmp_path);
}

const std::vector<std::filesystem::path> kTiffInputs = {
    "data/8bit.tif",
    "data/16bit.tif",
};

TEST_CASE("TIFF inputs") {
  xpano::pipeline::StitcherPipeline stitcher;
  auto result = stitcher.RunLoading(kTiffInputs, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.size() == 2);
  REQUIRE(!result.images[0].IsRaw());
  REQUIRE(result.images[1].IsRaw());

  auto preview0 = result.images[0].GetPreview();
  auto preview1 = result.images[1].GetPreview();
  CHECK(preview0.depth() == CV_8U);
  CHECK(preview1.depth() == CV_8U);
}

const std::filesystem::path kMalformedInput = "data/malformed.jpg";

TEST_CASE("Malformed input") {
  xpano::pipeline::StitcherPipeline stitcher;
  auto result = stitcher.RunLoading({kMalformedInput}, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  REQUIRE(result.images.empty());
  REQUIRE(result.matches.empty());
  REQUIRE(result.panos.empty());
}

#ifdef XPANO_WITH_MULTIBLEND
TEST_CASE("Stitcher pipeline Multiblend") {
  xpano::pipeline::StitcherPipeline stitcher;

  auto result = stitcher.RunLoading(kInputs, {}, {}).get();
  auto progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);

  const float eps = 0.02;
  auto stitch_algorithm = xpano::pipeline::StitchAlgorithmOptions{
      .blending_method = xpano::algorithm::BlendingMethod::kMultiblend};

  auto pano0 = stitcher
                   .RunStitching(result, {.pano_id = 0,
                                          .stitch_algorithm = stitch_algorithm})
                   .get()
                   .pano;
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);
  REQUIRE(pano0.has_value());
  CHECK_THAT(pano0->rows, WithinRel(804, eps));
  CHECK_THAT(pano0->cols, WithinRel(2145, eps));

  auto pano1 = stitcher
                   .RunStitching(result, {.pano_id = 1,
                                          .stitch_algorithm = stitch_algorithm})
                   .get()
                   .pano;
  progress = stitcher.Progress();
  CHECK(progress.tasks_done == progress.num_tasks);
  REQUIRE(pano1.has_value());
  CHECK_THAT(pano1->rows, WithinRel(976, eps));
  CHECK_THAT(pano1->cols, WithinRel(1335, eps));
}
#endif
