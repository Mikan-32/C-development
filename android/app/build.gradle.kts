plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "io.github.mikan32.nemunoki"

    compileSdk {
        version = release(36)
    }

    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "io.github.mikan32.nemunoki"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner =
            "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DNK_BUILD_TESTS=OFF"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            kotlin.directories += "../../platform/android/kotlin"
            res.srcDir("../../platform/android/res")

            manifest.srcFile(
                "../../platform/android/AndroidManifest.xml"
            )
        }
    }

    buildTypes {
        release {
            optimization {
                enable = true
                packageScope = setOf(
                    "androidx.**",
                    "kotlin.**",
                    "kotlinx.**"
                )
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}
