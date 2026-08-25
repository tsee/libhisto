{
  "targets": [
    {
      "target_name": "histo_native",
      "sources": [
        "src/addon.c",
        "../../src/histo.c",
        "../../src/histo2d.c",
        "../../src/fit.c",
        "../../src/kde.c",
        "../../src/simd.c",
        "../../src/sketch.c",
        "../../src/serialize.c",
        "../../src/serialize_2d.c",
        "../../src/simd_avx2.c",
        "../../src/simd_avx512.c",
        "../../src/simd_neon.c",
        "../../src/vendor/cJSON/cJSON.c",
        "../../tools/src/cli_common.c",
        "../../tools/src/cli_main.c",
        "../../tools/src/cli_palette.c",
        "../../tools/src/cmd_fill.c",
        "../../tools/src/cmd_plot.c",
        "../../tools/src/cmd_stats.c",
        "../../tools/src/cmd_fit.c",
        "../../tools/src/cmd_cmp.c",
        "../../tools/src/cmd_top.c",
        "../../tools/src/tui_term.c",
        "../../tools/src/tui_engine.c"
      ],
      "include_dirs": [
        "../../include",
        "../../src",
        "../../src/vendor/cJSON",
        "../../tools/include"
      ],
      "cflags": [
        "-std=c99",
        "-O3",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Wno-unused-parameter",
        "-Wno-unused-function"
      ],
      "conditions": [
        ["OS!='win'", {
          "libraries": ["-lm"]
        }]
      ]
    }
  ]
}
