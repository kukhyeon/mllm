macro(func_link_libaries target)
    target_link_libraries(${target} PUBLIC MLLM_CPU fmt::fmt-header-only)
    if (MLLM_OPENMP)
        target_compile_options(${target} PRIVATE -fopenmp)
        if (ARM)
            target_link_libraries(${target} PUBLIC -fopenmp -static-openmp)
        else ()
            target_link_libraries(${target} PUBLIC -fopenmp)
        endif ()
    endif ()
    if (QNN)
        target_compile_definitions(${target} PRIVATE USE_QNN)
        target_link_libraries(${target} PUBLIC MLLM_QNN ${CMAKE_DL_LIBS})
    endif()
    if (MLLM_BUILD_XNNPACK_BACKEND)
        target_link_libraries(${target} PRIVATE MllmXnnpackBackend)
    endif()
endmacro()

macro(func_llm_add_executable target)
    add_executable(${target}
        ${PROJECT_SOURCE_DIR}/main/${target}.cpp 
        ${DIR_SRC_CPU} 
        ${DIR_SRC_MEM_MANAGER} 
        ${DIR_SRC_EXP} 
        ${DIR_SRC}
        ${PROJECT_SOURCE_DIR}/src/tokenizers/Tokenizer.cpp
        ${PROJECT_SOURCE_DIR}/src/tokenizers/BPE/Bpe.cpp
        ${PROJECT_SOURCE_DIR}/src/tokenizers/WordPiece/WordPiece.cpp
        ${PROJECT_SOURCE_DIR}/src/tokenizers/Tiktoken/tiktoken.cpp
        ${PROJECT_SOURCE_DIR}/src/tokenizers/Unicode.cpp
        ${PROJECT_SOURCE_DIR}/src/tokenizers/UnicodeData.cpp
        ${PROJECT_SOURCE_DIR}/src/processor/PreProcess.cpp
        ${PROJECT_SOURCE_DIR}/src/utils/utils.cpp
        ${PROJECT_SOURCE_DIR}/src/utils/json.hpp
        ${PROJECT_SOURCE_DIR}/src/hardware/device.cpp
        ${PROJECT_SOURCE_DIR}/src/hardware/dvfs.cpp
        ${PROJECT_SOURCE_DIR}/src/hardware/record.cpp
    )
    func_link_libaries(${target})
endmacro()

# func_llm_add_executable(qwen_eval)
# func_llm_add_executable(test_qwen)
func_llm_add_executable(stream_qwen)
func_llm_add_executable(stream_qwen3)
func_llm_add_executable(stream_llama3)