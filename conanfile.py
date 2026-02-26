from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class EmailServerConan(ConanFile):
    name = "email-server"
    version = "1.0.0"
    description = "Complete Email Server Suite with POP3, IMAP, and SMTP"
    license = "Proprietary"
    url = "https://github.com/scorpiondefense/email_server"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "common/*",
        "pop3/*",
        "imap/*",
        "smtp/*",
        "tools/*",
        "tests/*",
        "config/*",
    )

    # Dependencies are system-installed (Boost, OpenSSL, SQLite3)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.variables["BUILD_TOOLS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["email_common"]
        self.cpp_info.set_property("cmake_file_name", "email_server")
        self.cpp_info.set_property("cmake_target_name", "email::email_common")
