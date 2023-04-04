#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "xpano/algorithm/algorithm.h"
#include "xpano/algorithm/image.h"
#include "xpano/gui/action.h"
#include "xpano/gui/panels/preview_pane.h"
#include "xpano/gui/panels/thumbnail_pane.h"
#include "xpano/pipeline/stitcher_pipeline.h"

namespace xpano::gui {

void DrawProgressBar(pipeline::ProgressReport progress);

cv::Mat DrawMatches(const algorithm::Match& match,
                    const std::vector<algorithm::Image>& images);

Action DrawMatchesMenu(const std::vector<algorithm::Match>& matches,
                       const ThumbnailPane& thumbnail_pane, int highlight_id);

Action DrawPanosMenu(const std::vector<algorithm::Pano>& panos,
                     const ThumbnailPane& thumbnail_pane, int highlight_id);

Action DrawMenu(pipeline::CompressionOptions* compression_options,
                pipeline::LoadingOptions* loading_options,
                pipeline::InpaintingOptions* inpaint_options,
                pipeline::MatchingOptions* matching_options,
                pipeline::StitchAlgorithmOptions* stitch_options,
                bool debug_enabled);

void DrawWelcomeTextPart1();

void DrawWelcomeTextPart2();

Action DrawImportActionButtons();

Action DrawActionButtons(ImageType image_type, int target_id,
                         algorithm::ProjectionType* projection_type);

}  // namespace xpano::gui
