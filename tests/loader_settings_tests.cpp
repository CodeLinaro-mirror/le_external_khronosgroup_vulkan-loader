/*
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation
 * Copyright (c) 2023 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and/or associated documentation files (the "Materials"), to
 * deal in the Materials without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Materials, and to permit persons to whom the Materials are
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included in
 * all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE
 * USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 * Author: Charles Giessen <charles@lunarg.com>
 */

#include "test_environment.h"

std::string get_settings_location(FrameworkEnvironment const& env) {
#if defined(WIN32)
    return env.get_folder(ManifestLocation::settings_location).location().str();
#elif COMMON_UNIX_PLATFORMS
    return "/home/fake_home/.local/share/vulkan/settings.d/vk_loader_settings.json";
#endif
}

// Make sure settings layer is found and that a layer defined in it is loaded
TEST(SettingsFile, FileExist) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* regular_layer_name = "VK_LAYER_TestLayer_0";
    env.add_explicit_layer(TestLayerDetails{ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(std::string(regular_layer_name))
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "regular_test_layer.json"}
                               .set_discovery_type(ManifestDiscoveryType::override_folder));
    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(regular_layer_name)
                .set_path(env.get_shimmed_layer_manifest_path().str())
                .set_control("on"))));
    {
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, regular_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();

        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto active_layer_props = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        EXPECT_TRUE(string_eq(active_layer_props.at(0).layerName, regular_layer_name));
    }
}

// Make sure that if the settings file is in a user local path, that it isn't used when running with elevated privileges
TEST(SettingsFile, RunningWithoutElevatedPermissions) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* regular_layer_name = "VK_LAYER_TestLayer_0";
    env.add_explicit_layer(TestLayerDetails{ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(std::string(regular_layer_name))
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "regular_test_layer.json"}
                               .set_discovery_type(ManifestDiscoveryType::override_folder));
    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(regular_layer_name)
                .set_path(env.get_layer_manifest_path().str())
                .set_control("on"))));
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    {
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, regular_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();

        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        env.debug_log.clear();
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, regular_layer_name));
    }
}

TEST(SettingsFile, RunningWithElevatedPermissions) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* regular_layer_name = "VK_LAYER_TestLayer_0";
    env.add_explicit_layer(TestLayerDetails{ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(std::string(regular_layer_name))
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "regular_test_layer.json"}
                               .set_discovery_type(ManifestDiscoveryType::override_folder));
    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name("VK_LAYER_haha3")
                .set_path(env.get_layer_manifest_path().str())
                .set_control("on"))));
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));

    env.platform_shim->set_elevated_privilege(true);

    ASSERT_NO_FATAL_FAILURE(env.GetLayerProperties(0));

    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.CheckCreate();

    ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    ASSERT_NO_FATAL_FAILURE(inst.GetActiveLayers(inst.GetPhysDev(), 0));
}

// Make sure settings file can have multiple sets of settings
TEST(SettingsFile, SupportsMultipleSetingsSimultaneously) {
    FrameworkEnvironment env{};
    const char* app_specific_layer_name = "VK_LAYER_TestLayer_0";
    env.add_explicit_layer(TestLayerDetails{ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(std::string(app_specific_layer_name))
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "VK_LAYER_app_specific.json"}
                               .set_discovery_type(ManifestDiscoveryType::override_folder));
    const char* global_layer_name = "VK_LAYER_TestLayer_1";
    env.add_explicit_layer(TestLayerDetails{ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(std::string(global_layer_name))
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "VK_LAYER_global.json"}
                               .set_discovery_type(ManifestDiscoveryType::override_folder));
    env.update_loader_settings(
        env.loader_settings
            // configuration that matches the current executable path - but dont set the app-key just yet
            .add_app_specific_setting(AppSpecificSettings{}
                                          .add_stderr_log_filter("all")
                                          .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                                                       .set_name(app_specific_layer_name)
                                                                       .set_path(env.get_layer_manifest_path(0).str())
                                                                       .set_control("on"))
                                          .add_app_key("key0"))
            // configuration that should never be used
            .add_app_specific_setting(
                AppSpecificSettings{}
                    .add_stderr_log_filter("all")
                    .add_layer_configuration(
                        LoaderSettingsLayerConfiguration{}.set_name("VK_LAYER_haha").set_path("/made/up/path").set_control("auto"))
                    .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                                 .set_name("VK_LAYER_haha2")
                                                 .set_path("/made/up/path2")
                                                 .set_control("auto"))
                    .add_app_key("key1")
                    .add_app_key("key2"))
            // Add a global configuration
            .add_app_specific_setting(AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
                LoaderSettingsLayerConfiguration{}
                    .set_name(global_layer_name)
                    .set_path(env.get_layer_manifest_path(1).str())
                    .set_control("on"))));
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    {
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, global_layer_name));

        // Check that the global config is used
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, global_layer_name));
    }
    env.debug_log.clear();
    // Set one set to contain the current executable path
    env.loader_settings.app_specific_settings.at(0).add_app_key(fs::fixup_backslashes_in_path(test_platform_executable_path()));
    env.update_loader_settings(env.loader_settings);
    {
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, app_specific_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, app_specific_layer_name));
    }
}

// Make sure layers found through the settings file are enableable by environment variables
TEST(SettingsFile, LayerAutoEnabledByEnvVars) {
    FrameworkEnvironment env{};
    env.loader_settings.set_file_format_version({1, 0, 0});
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* layer_name = "VK_LAYER_automatic";
    env.add_explicit_layer(
        TestLayerDetails{ManifestLayer{}.add_layer(
                             ManifestLayer::LayerDescription{}.set_name(layer_name).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                         "layer_name.json"}
            .set_discovery_type(ManifestDiscoveryType::override_folder));

    env.update_loader_settings(
        env.loader_settings.add_app_specific_setting(AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(layer_name)
                .set_path(env.get_layer_manifest_path(0).str())
                .set_control("auto"))));
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    {
        EnvVarWrapper instance_layers{"VK_INSTANCE_LAYERS", layer_name};
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, layer_name));
    }
    env.debug_log.clear();

    {
        EnvVarWrapper loader_layers_enable{"VK_LOADER_LAYERS_ENABLE", layer_name};
        auto layer_props = env.GetLayerProperties(1);
        EXPECT_TRUE(string_eq(layer_props.at(0).layerName, layer_name));
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, layer_name));
    }
}

// Make sure layers are disallowed from loading if the settings file says so
TEST(SettingsFile, LayerDisablesImplicitLayer) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* implicit_layer_name = "VK_LAYER_Implicit_TestLayer";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("oof")),
                           "implicit_test_layer.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(implicit_layer_name)
                .set_path(env.get_shimmed_layer_manifest_path(0).str())
                .set_control("off")
                .set_treat_as_implicit_manifest(true))));
    {
        ASSERT_NO_FATAL_FAILURE(env.GetLayerProperties(0));
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();

        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        ASSERT_NO_FATAL_FAILURE(inst.GetActiveLayers(inst.GetPhysDev(), 0));
    }
}

// Implicit layers should be reordered by the settings file
TEST(SettingsFile, ImplicitLayersDontInterfere) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* implicit_layer_name1 = "VK_LAYER_Implicit_TestLayer1";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name1)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("oof")),
                           "implicit_test_layer1.json");
    const char* implicit_layer_name2 = "VK_LAYER_Implicit_TestLayer2";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name2)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("oof")),
                           "implicit_test_layer2.json");
    // validate order when settings file is not present
    {
        auto layer_props = env.GetLayerProperties(2);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name2));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, implicit_layer_name2));
    }
    // Now setup the settings file to contain a specific order
    env.update_loader_settings(LoaderSettings{}.add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name1)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("auto")
                                         .set_treat_as_implicit_manifest(true))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name2)
                                         .set_path(env.get_shimmed_layer_manifest_path(1).str())
                                         .set_control("auto")
                                         .set_treat_as_implicit_manifest(true))));
    {
        auto layer_props = env.GetLayerProperties(2);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name2));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, implicit_layer_name2));
    }

    // Flip the order and store the settings in the env for later use in the test
    env.loader_settings = LoaderSettings{}.add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name2)
                                         .set_path(env.get_shimmed_layer_manifest_path(1).str())
                                         .set_control("auto")
                                         .set_treat_as_implicit_manifest(true))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name1)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("auto")
                                         .set_treat_as_implicit_manifest(true)));
    env.update_loader_settings(env.loader_settings);

    {
        auto layer_props = env.GetLayerProperties(2);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name1));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, implicit_layer_name1));
    }

    // Now add an explicit layer into the middle and verify that is in the correct location
    const char* explicit_layer_name3 = "VK_LAYER_Explicit_TestLayer3";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name3).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer3.json");
    env.loader_settings.app_specific_settings.at(0).layer_configurations.insert(
        env.loader_settings.app_specific_settings.at(0).layer_configurations.begin() + 1,
        LoaderSettingsLayerConfiguration{}
            .set_name(explicit_layer_name3)
            .set_path(env.get_shimmed_layer_manifest_path(2).str())
            .set_control("on"));
    env.update_loader_settings(env.loader_settings);
    {
        auto layer_props = env.GetLayerProperties(3);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, explicit_layer_name3));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, implicit_layer_name1));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 3);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, explicit_layer_name3));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, implicit_layer_name1));
    }
}

// Make sure layers that are disabled can't be enabled by the application
TEST(SettingsFile, ApplicationEnablesIgnored) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* explicit_layer_name = "VK_LAYER_TestLayer";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "regular_test_layer.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(explicit_layer_name)
                .set_path(env.get_shimmed_layer_manifest_path(0).str())
                .set_control("off"))));
    {
        ASSERT_NO_FATAL_FAILURE(env.GetLayerProperties(0));
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name);
        ASSERT_NO_FATAL_FAILURE(inst.CheckCreate(VK_ERROR_LAYER_NOT_PRESENT));
    }
}

TEST(SettingsFile, LayerListIsEmpty) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* implicit_layer_name = "VK_LAYER_TestLayer";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("HeeHee")),
                           "regular_test_layer.json");

    JsonWriter writer{};
    writer.StartObject();
    writer.AddKeyedString("file_format_version", "1.0.0");
    writer.StartKeyedObject("settings");
    writer.StartKeyedObject("layers");
    writer.EndObject();
    writer.EndObject();
    writer.EndObject();
    env.write_settings_file(writer.output);

    auto layer_props = env.GetLayerProperties(1);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name));

    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.CheckCreate();
    ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
    ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name));
}

// If a settings file exists but contains no valid settings - don't consider it
TEST(SettingsFile, InvalidSettingsFile) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* explicit_layer_name = "VK_LAYER_TestLayer";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "regular_test_layer.json");
    const char* implicit_layer_name = "VK_LAYER_ImplicitTestLayer";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("foobarbaz")),
                           "implicit_test_layer.json");
    auto check_integrity = [&env, explicit_layer_name, implicit_layer_name]() {
        auto layer_props = env.GetLayerProperties(2);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, explicit_layer_name));
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, explicit_layer_name));
    };

    // No actual settings
    {
        JsonWriter writer{};
        writer.StartObject();
        writer.AddKeyedString("file_format_version", "0.0.0");
        writer.EndObject();
        env.write_settings_file(writer.output);

        check_integrity();
    }

    {
        JsonWriter writer{};
        writer.StartObject();
        writer.AddKeyedString("file_format_version", "0.0.0");
        writer.StartKeyedArray("settings_array");
        writer.EndArray();
        writer.StartKeyedObject("settings");
        writer.EndObject();
        writer.EndObject();
        env.write_settings_file(writer.output);

        check_integrity();
    }

    {
        JsonWriter writer{};
        writer.StartObject();
        for (uint32_t i = 0; i < 3; i++) {
            writer.StartKeyedArray("settings_array");
            writer.EndArray();
            writer.StartKeyedObject("boogabooga");
            writer.EndObject();
            writer.StartKeyedObject("settings");
            writer.EndObject();
        }
        writer.EndObject();
        env.write_settings_file(writer.output);

        check_integrity();
    }
}

// Unknown layers are put in the correct location
TEST(SettingsFile, UnknownLayersInRightPlace) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});
    const char* explicit_layer_name1 = "VK_LAYER_TestLayer1";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name1).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "regular_test_layer1.json");
    const char* implicit_layer_name1 = "VK_LAYER_ImplicitTestLayer1";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name1)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("foobarbaz")),
                           "implicit_test_layer1.json");
    const char* explicit_layer_name2 = "VK_LAYER_TestLayer2";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name2).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "regular_test_layer2.json");
    const char* implicit_layer_name2 = "VK_LAYER_ImplicitTestLayer2";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name2)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("foobarbaz")),
                           "implicit_test_layer2.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name2)
                                         .set_path(env.get_shimmed_layer_manifest_path(2).str())
                                         .set_control("on"))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}.set_control("unordered_layer_location"))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name2)
                                         .set_path(env.get_shimmed_layer_manifest_path(3).str())
                                         .set_control("on")
                                         .set_treat_as_implicit_manifest(true))));

    auto layer_props = env.GetLayerProperties(4);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name2));
    ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name1));
    ASSERT_TRUE(string_eq(layer_props.at(2).layerName, explicit_layer_name1));
    ASSERT_TRUE(string_eq(layer_props.at(3).layerName, implicit_layer_name2));
    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.create_info.add_layer(explicit_layer_name1);
    inst.CheckCreate();
    ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 4);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name2));
    ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name1));
    ASSERT_TRUE(string_eq(layer_props.at(2).layerName, explicit_layer_name1));
    ASSERT_TRUE(string_eq(layer_props.at(3).layerName, implicit_layer_name2));
}

// Settings file allows loading multiple layers with the same name - as long as the path is different
TEST(SettingsFile, MultipleLayersWithSameName) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* explicit_layer_name = "VK_LAYER_TestLayer";
    env.add_explicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(explicit_layer_name)
                                                         .set_description("0000")
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                           "regular_test_layer1.json");

    env.add_explicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(explicit_layer_name)
                                                         .set_description("1111")
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                           "regular_test_layer2.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("on"))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(1).str())
                                         .set_control("on"))));
    auto layer_props = env.GetLayerProperties(2);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name));
    ASSERT_TRUE(string_eq(layer_props.at(0).description, "0000"));
    ASSERT_TRUE(string_eq(layer_props.at(1).layerName, explicit_layer_name));
    ASSERT_TRUE(string_eq(layer_props.at(1).description, "1111"));
    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.CheckCreate();
    ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
    ASSERT_TRUE(string_eq(layers.at(0).layerName, explicit_layer_name));
    ASSERT_TRUE(string_eq(layers.at(0).description, "0000"));
    ASSERT_TRUE(string_eq(layers.at(1).layerName, explicit_layer_name));
    ASSERT_TRUE(string_eq(layers.at(1).description, "1111"));
}

// Settings file shouldn't be able to cause the same layer from the same path twice
TEST(SettingsFile, MultipleLayersWithSamePath) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* explicit_layer_name = "VK_LAYER_TestLayer";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "regular_test_layer1.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("on"))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("on"))));

    auto layer_props = env.GetLayerProperties(1);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name));

    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.CheckCreate();
    ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
    ASSERT_TRUE(string_eq(layers.at(0).layerName, explicit_layer_name));
}

// Settings contains a layer whose name doesn't match the one found in the layer manifest - make sure the layer from the settings
// file is removed
TEST(SettingsFile, MismatchedLayerNameAndManifestPath) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* manifest_explicit_layer_name = "VK_LAYER_MANIFEST_TestLayer";
    const char* settings_explicit_layer_name = "VK_LAYER_Settings_TestLayer";
    env.add_explicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(manifest_explicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                           "regular_test_layer1.json");

    const char* implicit_layer_name = "VK_LAYER_Implicit_TestLayer";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("oof")),
                           "implicit_test_layer.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(settings_explicit_layer_name)
                .set_path(env.get_shimmed_layer_manifest_path(0).str())
                .set_control("on"))));

    auto layer_props = env.GetLayerProperties(2);
    ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name));
    ASSERT_TRUE(string_eq(layer_props.at(1).layerName, manifest_explicit_layer_name));

    InstWrapper inst{env.vulkan_functions};
    FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
    inst.CheckCreate();
    ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
    auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
    ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name));
}

// Settings file should take precedence over the meta layer, if present
TEST(SettingsFile, MetaLayerAlsoActivates) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* settings_explicit_layer_name = "VK_LAYER_Regular_TestLayer";
    env.add_explicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(settings_explicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                           "explicit_test_layer.json");

    const char* settings_implicit_layer_name = "VK_LAYER_RegularImplicit_TestLayer";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(settings_implicit_layer_name)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("AndISaidHey")
                                                         .set_enable_environment("WhatsGoingOn")),
                           "implicit_layer.json");

    const char* component_explicit_layer_name1 = "VK_LAYER_Component_TestLayer1";
    env.add_explicit_layer(TestLayerDetails(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(component_explicit_layer_name1)
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "component_test_layer1.json"));

    const char* component_explicit_layer_name2 = "VK_LAYER_Component_TestLayer2";
    env.add_explicit_layer(TestLayerDetails(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                                          .set_name(component_explicit_layer_name2)
                                                                          .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
                                            "component_test_layer2.json"));

    const char* meta_layer_name1 = "VK_LAYER_meta_layer1";
    env.add_implicit_layer(ManifestLayer{}
                               .set_file_format_version(ManifestVersion(1, 1, 2))
                               .add_layer(ManifestLayer::LayerDescription{}
                                              .set_name(meta_layer_name1)
                                              .add_component_layer(component_explicit_layer_name2)
                                              .add_component_layer(component_explicit_layer_name1)
                                              .set_disable_environment("NotGonnaWork")),
                           "meta_test_layer.json");

    const char* meta_layer_name2 = "VK_LAYER_meta_layer2";
    env.add_implicit_layer(ManifestLayer{}
                               .set_file_format_version(ManifestVersion(1, 1, 2))
                               .add_layer(ManifestLayer::LayerDescription{}
                                              .set_name(meta_layer_name2)
                                              .add_component_layer(component_explicit_layer_name1)
                                              .set_disable_environment("ILikeTrains")
                                              .set_enable_environment("BakedBeans")),
                           "not_automatic_meta_test_layer.json");

    env.update_loader_settings(env.loader_settings.set_file_format_version({1, 0, 0}).add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(settings_explicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_control("on")
                                         .set_treat_as_implicit_manifest(false))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}.set_control("unordered_layer_location"))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(settings_implicit_layer_name)
                                         .set_path(env.get_shimmed_layer_manifest_path(1).str())
                                         .set_control("auto")
                                         .set_treat_as_implicit_manifest(true))));
    {
        EnvVarWrapper enable_meta_layer{"WhatsGoingOn", "1"};
        auto layer_props = env.GetLayerProperties(6);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, settings_explicit_layer_name));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, meta_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, meta_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(3).layerName, component_explicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(4).layerName, component_explicit_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(5).layerName, settings_implicit_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 5);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, settings_explicit_layer_name));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, component_explicit_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, component_explicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(3).layerName, meta_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(4).layerName, settings_implicit_layer_name));
    }
    {
        EnvVarWrapper enable_meta_layer{"BakedBeans", "1"};
        auto layer_props = env.GetLayerProperties(6);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, settings_explicit_layer_name));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, meta_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, meta_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(3).layerName, component_explicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(4).layerName, component_explicit_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(5).layerName, settings_implicit_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 5);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, settings_explicit_layer_name));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, component_explicit_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, component_explicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(3).layerName, meta_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(4).layerName, meta_layer_name2));
    }
}

// Layers are correctly ordered by settings file.
TEST(SettingsFile, LayerOrdering) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* explicit_layer_name1 = "VK_LAYER_Regular_TestLayer1";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name1).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer1.json");

    const char* explicit_layer_name2 = "VK_LAYER_Regular_TestLayer2";
    env.add_explicit_layer(
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name2).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer2.json");

    const char* implicit_layer_name1 = "VK_LAYER_Implicit_TestLayer1";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name1)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("Domierigato")),
                           "implicit_layer1.json");

    const char* implicit_layer_name2 = "VK_LAYER_Implicit_TestLayer2";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name2)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("Mistehrobato")),
                           "implicit_layer2.json");

    env.loader_settings.add_app_specific_setting(AppSpecificSettings{}.add_stderr_log_filter("all"));

    std::vector<LoaderSettingsLayerConfiguration> layer_configs{4};
    layer_configs.at(0)
        .set_name(explicit_layer_name1)
        .set_path(env.get_shimmed_layer_manifest_path(0).str())
        .set_control("on")
        .set_treat_as_implicit_manifest(false);
    layer_configs.at(1)
        .set_name(explicit_layer_name2)
        .set_path(env.get_shimmed_layer_manifest_path(1).str())
        .set_control("on")
        .set_treat_as_implicit_manifest(false);
    layer_configs.at(2)
        .set_name(implicit_layer_name1)
        .set_path(env.get_shimmed_layer_manifest_path(2).str())
        .set_control("on")
        .set_treat_as_implicit_manifest(true);
    layer_configs.at(3)
        .set_name(implicit_layer_name2)
        .set_path(env.get_shimmed_layer_manifest_path(3).str())
        .set_control("on")
        .set_treat_as_implicit_manifest(true);

    std::sort(layer_configs.begin(), layer_configs.end());
    uint32_t permutation_count = 0;
    do {
        env.loader_settings.app_specific_settings.at(0).layer_configurations.clear();
        env.loader_settings.app_specific_settings.at(0).add_layer_configurations(layer_configs);
        env.update_loader_settings(env.loader_settings);

        auto layer_props = env.GetLayerProperties(4);
        for (uint32_t i = 0; i < 4; i++) {
            ASSERT_TRUE(layer_configs.at(i).name == layer_props.at(i).layerName);
        }

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto active_layers = inst.GetActiveLayers(inst.GetPhysDev(), 4);
        for (uint32_t i = 0; i < 4; i++) {
            ASSERT_TRUE(layer_configs.at(i).name == active_layers.at(i).layerName);
        }
        env.debug_log.clear();
        permutation_count++;
    } while (std::next_permutation(layer_configs.begin(), layer_configs.end()));
    ASSERT_EQ(permutation_count, 24U);  // should be this many orderings
}

TEST(SettingsFile, EnvVarsWork_VK_LAYER_PATH) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* explicit_layer_name1 = "VK_LAYER_Regular_TestLayer1";
    env.add_explicit_layer(TestLayerDetails{
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name1).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer1.json"}
                               .set_discovery_type(ManifestDiscoveryType::env_var));

    const char* implicit_layer_name1 = "VK_LAYER_Implicit_TestLayer1";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name1)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("Domierigato")),
                           "implicit_layer1.json");
    const char* non_env_var_layer_name2 = "VK_LAYER_Regular_TestLayer2";
    env.add_explicit_layer(TestLayerDetails{
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(non_env_var_layer_name2).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer2.json"});

    {
        auto layer_props = env.GetLayerProperties(2);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, explicit_layer_name1));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
    }
    {
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name1);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, explicit_layer_name1));
    }
    env.update_loader_settings(
        env.loader_settings.add_app_specific_setting(AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(non_env_var_layer_name2)
                .set_control("on")
                .set_path(env.get_shimmed_layer_manifest_path(2).str()))));
    {
        auto layer_props = env.GetLayerProperties(3);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, explicit_layer_name1));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, implicit_layer_name1));
    }
    {
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name1);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 3);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, explicit_layer_name1));
    }
}

TEST(SettingsFile, EnvVarsWork_VK_ADD_LAYER_PATH) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* implicit_layer_name1 = "VK_LAYER_Implicit_TestLayer1";
    env.add_implicit_layer(ManifestLayer{}.add_layer(ManifestLayer::LayerDescription{}
                                                         .set_name(implicit_layer_name1)
                                                         .set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)
                                                         .set_disable_environment("Domierigato")),
                           "implicit_layer1.json");
    const char* explicit_layer_name1 = "VK_LAYER_Regular_TestLayer1";
    env.add_explicit_layer(TestLayerDetails{
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name1).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer1.json"}
                               .set_discovery_type(ManifestDiscoveryType::add_env_var));
    const char* non_env_var_layer_name2 = "VK_LAYER_Regular_TestLayer2";
    env.add_explicit_layer(TestLayerDetails{
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(non_env_var_layer_name2).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer2.json"});

    {
        auto layer_props = env.GetLayerProperties(3);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, explicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, non_env_var_layer_name2));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
    }
    {
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name1);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 2);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, implicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, explicit_layer_name1));
    }

    env.update_loader_settings(env.loader_settings.add_app_specific_setting(
        AppSpecificSettings{}
            .add_stderr_log_filter("all")
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(explicit_layer_name1)
                                         .set_control("on")
                                         .set_path(env.get_shimmed_layer_manifest_path(1).str()))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(non_env_var_layer_name2)
                                         .set_control("on")
                                         .set_path(env.get_shimmed_layer_manifest_path(2).str()))
            .add_layer_configuration(LoaderSettingsLayerConfiguration{}
                                         .set_name(implicit_layer_name1)
                                         .set_control("on")
                                         .set_path(env.get_shimmed_layer_manifest_path(0).str())
                                         .set_treat_as_implicit_manifest(true))));
    {
        auto layer_props = env.GetLayerProperties(3);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name1));
        ASSERT_TRUE(string_eq(layer_props.at(1).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layer_props.at(2).layerName, implicit_layer_name1));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 3);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, explicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, implicit_layer_name1));
    }
    {
        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.create_info.add_layer(explicit_layer_name1);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layers = inst.GetActiveLayers(inst.GetPhysDev(), 3);
        ASSERT_TRUE(string_eq(layers.at(0).layerName, explicit_layer_name1));
        ASSERT_TRUE(string_eq(layers.at(1).layerName, non_env_var_layer_name2));
        ASSERT_TRUE(string_eq(layers.at(2).layerName, implicit_layer_name1));
    }
}

TEST(SettingsFile, EnvVarsWork_VK_INSTANCE_LAYERS) {
    FrameworkEnvironment env{};
    env.add_icd(TestICDDetails(TEST_ICD_PATH_VERSION_2));
    env.get_test_icd().add_physical_device({});

    const char* explicit_layer_name = "VK_LAYER_Regular_TestLayer1";
    env.add_explicit_layer(TestLayerDetails{
        ManifestLayer{}.add_layer(
            ManifestLayer::LayerDescription{}.set_name(explicit_layer_name).set_lib_path(TEST_LAYER_PATH_EXPORT_VERSION_2)),
        "explicit_test_layer1.json"});
    {
        auto layer_props = env.GetLayerProperties(1);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        ASSERT_NO_FATAL_FAILURE(inst.GetActiveLayers(inst.GetPhysDev(), 0));
    }

    {
        EnvVarWrapper vk_instance_layers{"VK_INSTANCE_LAYERS", explicit_layer_name};
        auto layer_props = env.GetLayerProperties(1);
        ASSERT_TRUE(string_eq(layer_props.at(0).layerName, explicit_layer_name));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_FALSE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        auto layer = inst.GetActiveLayers(inst.GetPhysDev(), 1);
        ASSERT_TRUE(string_eq(layer.at(0).layerName, explicit_layer_name));
    }
    env.update_loader_settings(
        env.loader_settings.add_app_specific_setting(AppSpecificSettings{}.add_stderr_log_filter("all").add_layer_configuration(
            LoaderSettingsLayerConfiguration{}
                .set_name(explicit_layer_name)
                .set_control("off")
                .set_path(env.get_shimmed_layer_manifest_path(0).str()))));
    {
        ASSERT_NO_FATAL_FAILURE(env.GetLayerProperties(0));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        ASSERT_NO_FATAL_FAILURE(inst.GetActiveLayers(inst.GetPhysDev(), 0));
    }
    {
        EnvVarWrapper vk_instance_layers{"VK_INSTANCE_LAYERS", explicit_layer_name};
        ASSERT_NO_FATAL_FAILURE(env.GetLayerProperties(0));

        InstWrapper inst{env.vulkan_functions};
        FillDebugUtilsCreateDetails(inst.create_info, env.debug_log);
        inst.CheckCreate();
        ASSERT_TRUE(env.debug_log.find(std::string("Using settings found in ") + get_settings_location(env)));
        ASSERT_NO_FATAL_FAILURE(inst.GetActiveLayers(inst.GetPhysDev(), 0));
    }
}

// VK_LOADER_LAYERS_ENABLE
// VK_LOADER_LAYERS_DISABLE

#if defined(WIN32)
TEST(SettingsFile, MultipleKeysInRegistry) {}
#endif

// Preinstance functions respect the settings file
TEST(SettingsFile, PreInstanceFunctions) {}

// If an implicit layer's disable environment variable is set, but the settings file says to turn the layer on, the layer should be
// activated.
TEST(SettingsFile, ImplicitLayerDisableEnvironmentVariableOverriden) {}

// Settings can say which filters to use - make sure those are propagated & treated correctly
TEST(SettingsFile, StderrLogFilters) {}

// Enough layers exist that arrays need to be resized - make sure that works
TEST(SettingsFile, TooManyLayers) {}
