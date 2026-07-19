//#include "docmind/modules/FileTranslationModule.hpp"
//#include <iostream>
//
//int main() {
//    try {
//        // 使用模块直接处理
//        docmind::FileTranslationModule module("英文", true, true, 0.5f, 200);
//        std::string out = module.process("tests/test_excel.xlsx", "");
//        std::cout << "Output saved to: " << out << std::endl;
//
//        // 也可以使用统一入口
//        // std::string out2 = docmind::process_file("test.pdf", "", "", "中文");
//        // std::cout << "Output: " << out2 << std::endl;
//    } catch (const std::exception& e) {
//        std::cerr << "Error: " << e.what() << std::endl;
//    }
//    return 0;
//}

//#include "docmind/DocumentEngine.h"
//#include <iostream>

//int main() {
//    try {
//        std::string out = process_photo("tests/test2.png", "", "", "中文",512);
//        std::cout << "Saved to: " << out << std::endl;
//    } catch (const std::exception& e) {
//        std::cerr << e.what() << std::endl;
//    }
//    return 0;
//}

#include "docmind/DocumentEngine.h"
#include <iostream>
int main() {
    std::string translated = translate_text("Request received\n"
                                            "Submitted on Jul 15, 6:06 PM\n"
                                            "We’ll review your account\n"
                                            "You’re next in line for review\n"
                                            "3\n"
                                            "We’ll email you the outcome", "中文");
    std::cout << translated << std::endl;
    return 0;
}

//#include "docmind/DocumentEngine.h"
//#include <iostream>
//int main() {
//    cv::Mat img = cv::imread("tests/test2_trans.png");
//    std::string result = translate_screenshot_image(img, "英文");
//    std::cout << "Translated text: " << result << std::endl;
//    return 0;
//}
