#include "algorithm/algorithm.h"

#include <optional>
#include <utility>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/stitching.hpp>

#include "algorithm/image.h"
#include "constants.h"

namespace xpano::algorithm {

std::vector<cv::DMatch> MatchImages(const Image& img1, const Image& img2) {
  if (img1.GetKeypoints().empty() || img2.GetKeypoints().empty()) {
    return {};
  }

  // KNN MATCH, K = 2
  cv::FlannBasedMatcher matcher;
  std::vector<std::vector<cv::DMatch>> matches;
  matcher.knnMatch(img1.GetDescriptors(), img2.GetDescriptors(), matches, 2);

  // FILTER BY FIRST/SECOND RATIO
  std::vector<cv::DMatch> good_matches;
  for (const auto& match : matches) {
    double ratio = match[0].distance / match[1].distance;
    if (ratio < 0.8) {
      good_matches.push_back(match[0]);
    }
  }

  if (good_matches.size() < 4) {
    return {};
  }

  // ESTIMATE HOMOGRAPHY
  int num_good_matches = static_cast<int>(good_matches.size());
  cv::Mat src_points(1, num_good_matches, CV_32FC2);
  cv::Mat dst_points(1, num_good_matches, CV_32FC2);
  cv::Mat dst_points_proj;
  int idx = 0;
  for (cv::DMatch& match : good_matches) {
    src_points.at<cv::Vec2f>(0, idx) = img1.GetKeypoints()[match.queryIdx].pt;
    dst_points.at<cv::Vec2f>(0, idx) = img2.GetKeypoints()[match.trainIdx].pt;
    idx++;
  }
  cv::Mat h_mat = cv::findHomography(src_points, dst_points, cv::RANSAC, 3);
  if (h_mat.empty()) {
    return {};
  }
  perspectiveTransform(src_points, dst_points_proj, h_mat);

  // FILTER OUTLIERS
  std::vector<cv::DMatch> inliers;
  for (int i = 0; i < good_matches.size(); i++) {
    cv::Vec2f diff =
        dst_points.at<cv::Vec2f>(0, i) - dst_points_proj.at<cv::Vec2f>(0, i);
    if (norm(diff) < 3) {
      inliers.push_back(good_matches[i]);
    }
  }

  return inliers;
}

std::vector<Pano> FindPanos(const std::vector<Match>& matches) {
  using MMatch = std::pair<int, int>;

  std::vector<MMatch> good_matches;
  for (const auto& match : matches) {
    if (match.matches.size() > kMatchThreshold) {
      good_matches.emplace_back(match.id1, match.id2);
    }
  }

  if (good_matches.empty()) {
    return {};
  }

  std::vector<Pano> result;
  Pano next{};
  next.ids.push_back(good_matches[0].first);
  next.ids.push_back(good_matches[0].second);

  for (int i = 1; i < good_matches.size(); i++) {
    const auto& match = good_matches[i];
    if (next.ids.back() == match.first) {
      next.ids.push_back(match.second);
    } else {
      result.push_back(next);
      next.ids.resize(0);
      next.ids.push_back(match.first);
      next.ids.push_back(match.second);
    }
  }
  result.push_back(next);
  return result;
}

std::pair<cv::Stitcher::Status, cv::Mat> Stitch(const std::vector<cv::Mat>& images) {
  cv::Mat out;
  cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(cv::Stitcher::PANORAMA);
  cv::Stitcher::Status status = stitcher->stitch(images, out);
  
  return std::make_pair(status, out);
}

std::string ToString(cv::Stitcher::Status& status) {
  switch(status) {
    case cv::Stitcher::OK:  return "OK";
    case cv::Stitcher::ERR_NEED_MORE_IMGS:  return "ERR_NEED_MORE_IMGS";
    case cv::Stitcher::ERR_HOMOGRAPHY_EST_FAIL:  return "ERR_HOMOGRAPHY_EST_FAIL";
    case cv::Stitcher::ERR_CAMERA_PARAMS_ADJUST_FAIL:  return "ERR_CAMERA_PARAMS_ADJUST_FAIL";
    default:  return "ERR_UNKNOWN";
  }
  return "ERR_UNKNOWN";
}

}  // namespace xpano::algorithm
