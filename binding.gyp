{
  "targets": [
    {
      "target_name": "knit_decoder",
      "sources": ["src/main.cpp", "src/context.hpp", "src/worker.hpp"],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ],
      "cflags_cc!": ["-fno-rtti", "-fno-exceptions"],
      "cflags!": ["-fno-exceptions", "-Wall", "-std=c++11"],
      "xcode_settings": {
        "OTHER_CPLUSPLUSFLAGS" : ["-std=c++11","-stdlib=libc++"],
        "OTHER_LDFLAGS": ["-stdlib=libc++"],
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
      },
      "conditions": [
        ['OS=="mac"', {
          "include_dirs": [
            "/usr/local/include"
          ],
          "libraries" : [
            "-lavcodec",
            "-lavformat",
            "-lswscale",
            "-lavutil"
          ]
        }],
        ['OS=="linux"', {
          "include_dirs": [
            "/usr/local/include"
          ],
          "libraries" : [
            "-lavcodec",
            "-lavformat",
            "-lswscale",
            "-lavutil",
          ]
        }],
        ['OS=="win"', {
          "include_dirs": [
            "$(LIBAV_PATH)include"
          ],
          "libraries" : [
            "-l$(LIBAV_PATH)avcodec",
            "-l$(LIBAV_PATH)avformat",
            "-l$(LIBAV_PATH)swscale",
            "-l$(LIBAV_PATH)avutil"
          ]
        }]
      ],
    }
  ]
}
