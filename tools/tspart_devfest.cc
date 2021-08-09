#include "external/tinyfiledialogs.h"
#include "FunctionAPI/Graph.hh"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <TGUI/TGUI.hpp>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "external/nanosvg.h"
#include "external/nanosvgrast.h"

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cmath>
#include <cstdlib>

const int PANEL_HEIGHT = 160;


sf::Texture sf_tex;


std::shared_ptr<std::string> getImageFile()
{
    std::vector<const char*> filePatterns = {
            "*.png", "*.jpg", "*.bmp"
    };

    auto result = tinyfd_openFileDialog(
            "Choose an image",
            "",
            filePatterns.size(),
            filePatterns.data(),
            "Images",
            0
    );

    if (result == nullptr)
        return nullptr;
    return std::make_shared<std::string>(result);
}

std::shared_ptr<std::string> saveImageFile()
{
    std::vector<const char*> filePatterns = {
            "*.svg"
    };

    auto result = tinyfd_saveFileDialog(
            "Save an image",
            "",
            filePatterns.size(),
            filePatterns.data(),
            "SVG file"
    );

    if (result == nullptr)
        return nullptr;
    return std::make_shared<std::string>(result);
}


tgui::VerticalLayout::Ptr named_column(
  std::vector<std::pair<const char*, tgui::Widget::Ptr>> widgets,
  int whitespace=0)
{
  auto layout = tgui::VerticalLayout::create();
  for (auto& p : widgets)
  {
    auto row = tgui::HorizontalLayout::create();
    tgui::Label::Ptr label = tgui::Label::create();
    label->setText(p.first);
    label->setTextSize(10);
    label->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Right);
    label->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
    row->add(label);
    row->add(p.second);
    row->setRatio(0, 0.35);
    layout->add(row);
    layout->addSpace(0.1);
  }
  layout->addSpace(1.1f*whitespace);
  return layout;
}

tgui::HorizontalLayout::Ptr slider(int from, int def, int to, std::function<void(int)> callback)
{
  auto lt = tgui::HorizontalLayout::create();

  tgui::Slider::Ptr slider = tgui::Slider::create();
  tgui::EditBox::Ptr box = tgui::EditBox::create();

  auto update = [=](int val)->void
  {
    int new_val = std::min(std::max(from, val), to);
    if (int(slider->getValue()) != new_val)
      slider->setValue(new_val);
    if (val >= from && box->getText() != std::to_string(new_val))
      box->setText(std::to_string(new_val));
    callback(new_val);
  };

  box->setInputValidator(tgui::EditBox::Validator::Int);
  box->connect("TextChanged", [=]() {
    std::string val = box->getText();
    if (val.size() == 0)
      return;
    int r = std::stoi(val);
    if (r < from)
      return;
    update(r);
  });

  slider->setMinimum(from);
  slider->setMaximum(to);
  slider->connect("ValueChanged", [=]() {
    update(slider->getValue());
  });

  update(def);

  lt->add(slider);
  lt->add(box);
  lt->setRatio(0, 0.7);
  lt->setRatio(1, 0.3);
   return lt;
}



int main (int argc, char** argv)
{
  if (argc != 4) {
    printf("Missing args: <file in> <algo> <file out.svg>");
    return -1;
  }

  sf::RenderWindow window(
    sf::VideoMode(1280, 720),
    "TSPArt",
    sf::Style::Titlebar | sf::Style::Resize | sf::Style::Close
  );

  auto gr = Graph<ImageMixin, PointsMixin>();
  gr.logger.set_log_level(Logger::Level::Verbose);

  auto& in_filename = gr.input<std::string>();
  auto& out_filename = gr.input<std::string>();
  auto& size = gr.input<size_t>();

  auto& gauss = gr.input<size_t>();
  auto& steepness = gr.input<float>();
  auto& graypoint = gr.input<float>();
  auto& log = gr.input<float>();

  auto& fill = gr.input<size_t>();

  auto& in = gr.image_maximizer(gr.image_loader(in_filename), size);
  auto& pre = gr.image_filter_logarithm(
    gr.image_normalization(
      gr.image_filter_grayscale(in),
      steepness,
      graypoint,
      gauss),
    log);

  auto& scalar = gr.grayscale_image_to_scalar_field(gr.image_filter_inverse(pre), 1);
  auto& pref = gr.scalar_field_mass_prefix_sum(scalar);

  auto& pts = gr.n_voronoi_relaxation(
    gr.points_generator(scalar, fill),
    pref,
    5);

  auto& mst = gr.mst_ordering(pts);
  auto& skip = gr.skip_ordering(pts);
  auto& hilbert = gr.hilbert_points_orderer(pts);
  auto& nearest_neighbour = gr.nearest_neighbour_points_orderer(pts);

  auto& pln_saver = gr.polyline_svg_saver(mst, out_filename);


  auto new_file = [=, &in_filename](std::string path) -> void
  {
      in_filename.set_data(path);

  };

  out_filename.set_data(argv[3]);

  // Size: 2000
  size.set_data(1500);

  // Details: 8
  gauss.set_data(pow(1.5, 8));

  // Contrast: 8
  steepness.set_data(pow(1.5, 10 - 8));

  // Brightness: 0
  graypoint.set_data(127.f - 0 * 2.f);

  // Log-gamma: 5
  log.set_data(pow(2, 5));

  // density : 9
  fill.set_data(int(12*pow(1.5, 10 - 9)));


  DataPromise<Polyline>* tgt = nullptr;
  if (atoi(argv[2]) == 1) {
    tgt = &static_cast<DataPromise<Polyline>&>(mst);
  } else if (atoi(argv[2]) == 2) {
    tgt = &static_cast<DataPromise<Polyline>&>(skip);
  } else if (atoi(argv[2]) == 3) {
    tgt = &static_cast<DataPromise<Polyline>&>(nearest_neighbour);
  } else {
    tgt = &static_cast<DataPromise<Polyline>&>(hilbert);
  }
  pln_saver.in.connect(*tgt);

  new_file(std::string(argv[1]));
  pln_saver.update();
  exit(0);

}
