# Third-party components

- **Tracktion Engine** (GPLv3): see TRACKTION_LICENSE.md
- **JUCE** (GPLv3 / commercial)
- **demucs.cpp** by Sevag Hanssian (MIT): https://github.com/sevagh/demucs.cpp, the C++ inference code for Demucs v4, used for stem separation
- **Eigen 3.4** (MPL-2.0): https://eigen.tuxfamily.org, the linear algebra library used by demucs.cpp
- **Demucs model weights** (Meta AI) are **not** distributed with this app. The app downloads them on first use, with your consent, from
  https://huggingface.co/datasets/Retrobear/demucs.cpp. Meta released the weights for research and non-commercial use.
