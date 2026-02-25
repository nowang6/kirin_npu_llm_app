#include "napi/native_api.h"
#include <hilog/log.h>
#include <string>
#include <vector>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <thread>
#include "include/cann_llm_engine_context.h"
#include "include/cann_llm_engine_executor.h"
#include "include/common/lm_engine_model_info.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0x0000

#undef LOG_TAG
#define LOG_TAG "LLM_DEMO"

using namespace std;

struct LLMEngineProcessParam {
    std::string contextJsonPath = "";
    std::string executorJsonPath = "";
    bool isAsync = true;
    std::string prompt = "";
};

static napi_threadsafe_function g_tsf = nullptr;
static napi_env g_env = nullptr;

struct CallbackData {
    napi_ref callback = nullptr;
    string question = ""; 
};

bool g_LLMEngineGenerateAsyncFailed = false;
bool g_LLMEngineWaitFlag = false;
bool g_LLMEngineIsFirstToken = true;

LLMEngineProcessParam param;

LLMEngine_Context* g_LLMEngineContext = nullptr;
LLMEngine_Executor* g_LLMEngineExecutor = nullptr;

/** Qwen3 ChatML 单轮对话模板（无 tools）：默认 system + user + assistant 前缀，启用 thinking */
static std::string FormatPromptWithChatTemplate(const std::string& userInput)
{
    std::string out;
    out += "<|im_start|>system\nYou are Qwen, created by Alibaba Cloud. You are a helpful assistant.<|im_end|>\n";
    out += "<|im_start|>user\n";
    out += userInput;
    out += "<|im_end|>\n";
    out += "<|im_start|>assistant\n";
    out += "<think>\n\n</think>\n\n";
    return out;
}

static void InitParam()
{
    param.contextJsonPath = "/data/storage/el2/base/files/context.json";
    param.executorJsonPath = "/data/storage/el2/base/files/executor.json";
    param.isAsync = true;
    param.prompt = "";
}

static int LLMEngineContextGenerate(const char* json_path)
{
    FILE* file = fopen(json_path, "r");
    if (file == nullptr) {
        printf("Failed to open JSON file!\n");
        return -1;
    }

    // 读取文件内容到内存
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* jsonStr = (reinterpret_cast <char*>(malloc(file_size + 1)));
    if (jsonStr == nullptr) {
        printf("Failed to allocate memory for JSON string!\n");
        fclose(file);
        return -1;
    }
    fread(jsonStr, 1, file_size, file);
    fclose(file);
    jsonStr[file_size] = '\0';  // 确保字符串以空字符结尾

    g_LLMEngineContext = LLMEngine_Context_CreateFromContextJson(jsonStr);
    if (g_LLMEngineContext == nullptr) {
        printf("Failed to create context from JSON!\n");
        free(jsonStr);
        return -1;
    }
    free(jsonStr);
    return 0;
}

int LLMEngineExecutorGenerate(const char* json_path)
{
    FILE* file = fopen(json_path, "r");
    if (file == nullptr) {
        printf("Failed to open JSON file!\n");
        return -1;
    }

    // 读取文件内容到内存
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* jsonStr = (reinterpret_cast <char*>(malloc(file_size + 1)));
    if (jsonStr == nullptr) {
        printf("Failed to allocate memory for JSON string!\n");
        fclose(file);
        return -1;
    }
    fread(jsonStr, 1, file_size, file);
    fclose(file);
    jsonStr[file_size] = '\0';  // 确保字符串以空字符结尾

    g_LLMEngineExecutor = LLMEngine_Executor_CreateFromJson(jsonStr);
    if (g_LLMEngineExecutor == nullptr) {
        printf("Failed to create exexutor from JSON!\n");
        free(jsonStr);
        return -1;
    }
    free(jsonStr);
    return 0;
}

static int LLMEngineInit(LLMEngineProcessParam& param)
{
    // init and create
    OH_LOG_INFO(LOG_APP, "LLM Engine, Init...");

    OH_LOG_INFO(LOG_APP, "LLMEngine_Context_CreateFromContextJson, start...");
    if (LLMEngineContextGenerate(param.contextJsonPath.c_str()) != 0) {
        OH_LOG_ERROR(LOG_APP, "LLMEngineContextGenerate failed.");
        return -1;
    }

    OH_LOG_INFO(LOG_APP, "LLMEngine_Executor_CreateFromJson, start...");
    if (LLMEngineExecutorGenerate(param.executorJsonPath.c_str()) != 0) {
        OH_LOG_ERROR(LOG_APP, "LLMEngineExecutorGenerate failed.");
        return -1;
    }

    OH_LOG_INFO(LOG_APP, "LLM Engine Init Done.");
    return 0;
}

static bool LLMEngineGenerateSync(LLMEngine_Context* llmEngineContext,
                                  LLMEngine_Executor* llmEngineExecutor, const std::string& llm_engine_prompt)
{
    LLMEngine_Status ret;
    std::string formatted = FormatPromptWithChatTemplate(llm_engine_prompt);
	LLMEngine_Prompt* mllmPrompt = LLMEngine_Prompt_Create();
	ret = LLMEngine_Prompt_SetText(mllmPrompt, formatted.c_str());
	if (ret != LLMEngine_SUCCESS)
	{
		OH_LOG_ERROR(LOG_APP, "LLMEngine_Prompt_SetTokenIds failed.");
        return false;
	}
    ret = LLMEngine_Executor_LLM_Generate(llmEngineExecutor, llmEngineContext, mllmPrompt);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Executor_LLM_Generate failed.");
        return false;
    }
    uint32_t len = 0;
    ret = LLMEngine_Context_GetAllGenerationLen(llmEngineContext, &len);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetAllGenerationLen failed.");
        return false;
    }

    char* generation = new char [len + 1];
    ret = LLMEngine_Context_GetAllGeneration(llmEngineContext, generation, len);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetAllGeneration failed.");
        return false;
    }

    generation[len] = '\0';
    OH_LOG_INFO(LOG_APP, "all generation len: %{public}d.", len);
    OH_LOG_INFO(LOG_APP, "all generation: %{public}s", generation);
    delete[] generation;
    double totalTimeMs = 0;
    ret = LLMEngine_Context_GetTotalTimeMs(llmEngineContext, &totalTimeMs);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetTotalTimeMs failed.");
        return true;
    }

    double prefillTimeMs = 0;
    ret = LLMEngine_Context_GetPrefillTimeMs(llmEngineContext, &prefillTimeMs);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetPrefillTimeMs failed.");
        return true;
    }

    double decodeTimeMs = 0;
    ret = LLMEngine_Context_GetDecodeTimeMs(llmEngineContext, &decodeTimeMs);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetDecodeTimeMs failed.");
        return true;
    }

    uint64_t inputTokenCount = 0;
    ret = LLMEngine_Context_GetInputTokenCount(llmEngineContext, &inputTokenCount);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetInputTokenCount failed.");
        return true;
    }

    uint64_t outputTokenCount = 0;
    ret = LLMEngine_Context_GetOutputTokenCount(llmEngineContext, &outputTokenCount);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOutputTokenCount failed.");
        return true;
    }
	
	uint64_t DecodeNum = 0;
	ret = LLMEngine_Context_GetDecodeNum(llmEngineContext, &DecodeNum);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetDecodeNum failed.");
        return true;
    }
    
    OH_LOG_INFO(LOG_APP, "inputTokenCount: %{public}lu.", inputTokenCount);
    OH_LOG_INFO(LOG_APP, "outputTokenCount: %{public}lu.", outputTokenCount);
	OH_LOG_INFO(LOG_APP, "DecodeNum: %{public}lu.", DecodeNum);
    OH_LOG_INFO(LOG_APP, "totalTimeMs: %{public}.5f ms.", totalTimeMs);
    OH_LOG_INFO(LOG_APP, "prefillTimeMs: %{public}.5f ms.", prefillTimeMs);
    OH_LOG_INFO(LOG_APP, "decodeTimeMs: %{public}.5f ms.", decodeTimeMs);
    OH_LOG_INFO(LOG_APP, "decodeTimeMs per token: %{public}.5f ms", decodeTimeMs / outputTokenCount);
    g_LLMEngineWaitFlag = false;
	LLMEngine_Prompt_Destroy(&mllmPrompt);
    return true;
}

static bool LLMEngineGenerateAsync(LLMEngine_Context* llmEngineContext,
                                   LLMEngine_Executor* llmEngineExecutor, const std::string& llm_engine_prompt)
{
    // 所有token生成完成回调函数
    void(*onAllTokensGenerateDoneFunc)(const LLMEngine_Context*) = [](const LLMEngine_Context* ctx)
    {
        uint32_t len = 0;
        auto ret = LLMEngine_Context_GetAllGenerationLen(ctx, &len);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetAllGenerationLen failed.");
            return;
        }
        char* generation = new char [len + 1];
        ret = LLMEngine_Context_GetAllGeneration(ctx, generation, len);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetAllGeneration failed.");
            return;
        }

        generation[len] = '\0';
        fflush(stdout);
        OH_LOG_INFO(LOG_APP, "all generation len: %d.", len);
        OH_LOG_INFO(LOG_APP, "all generation: %s", generation);
        delete[] generation;
        double totalTimeMs = 0;
        ret = LLMEngine_Context_GetTotalTimeMs(ctx, &totalTimeMs);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetTotalTimeMs failed.");
            return;
        }

        double prefillTimeMs = 0;
        ret = LLMEngine_Context_GetPrefillTimeMs(ctx, &prefillTimeMs);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetPreFillTimeMs failed.");
            return;
        }

        double decodeTimeMs = 0;
        ret = LLMEngine_Context_GetDecodeTimeMs(ctx, &decodeTimeMs);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetDecodeTimeMs failed.");
            return;
        }

        uint64_t inputTokenCount = 0;
        ret = LLMEngine_Context_GetInputTokenCount(ctx, &inputTokenCount);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetInputTokenCount failed.");
            return;
        }

        uint64_t outputTokenCount = 0;
        ret = LLMEngine_Context_GetOutputTokenCount(ctx, &outputTokenCount);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOutputTokenCount failed.");
            return;
        }
		
		uint64_t DecodeNum = 0;
		ret = LLMEngine_Context_GetDecodeNum(ctx, &DecodeNum);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetDecodeNum failed.");
            return;
        }
        
        OH_LOG_INFO(LOG_APP, "inputTokenCount: %{public}lu.", inputTokenCount);
        OH_LOG_INFO(LOG_APP, "outputTokenCount: %{public}lu.", outputTokenCount);
		OH_LOG_INFO(LOG_APP, "DecodeNum: %{public}lu.", DecodeNum);
        OH_LOG_INFO(LOG_APP, "totalTimeMs: %{public}5f ms.", totalTimeMs);
        OH_LOG_INFO(LOG_APP, "preFillTimeMs: %{public}.5f ms.", prefillTimeMs);
        OH_LOG_INFO(LOG_APP, "decodeTimeMs: %{public}.5f ms.", decodeTimeMs);
        OH_LOG_INFO(LOG_APP, "decodeTimeMs per token: %{public}.5f ms", decodeTimeMs / outputTokenCount);
        g_LLMEngineWaitFlag = false;
    };

    auto ret = LLMEngine_Context_SetOnAllTokensGenerateDoneFunc(llmEngineContext, onAllTokensGenerateDoneFunc);

    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_SetOnAllTokensGenerateDoneFunc failed.");
        return false;
    }

    // 部分token生成完成回调函数
    void(*onSomeTokenGenerateDoneFunc)(const LLMEngine_Context*) = [](const LLMEngine_Context* ctx)
    {
        uint32_t len = 0;
        auto ret = LLMEngine_Context_GetOneGenerationLen(ctx, &len);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOneGenerationLen failed.");
            return;
        }
        char* generation = new char [len + 1];
        ret = LLMEngine_Context_GetOneGeneration(ctx, generation, len);
        if (ret != LLMEngine_SUCCESS)
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOneGeneration failed.");
            return;
        }

        generation[len] = '\0';
        
        if (g_LLMEngineIsFirstToken)
        {
            OH_LOG_INFO(LOG_APP, "[INFO] curr generation: %{public}s", generation);
            napi_call_threadsafe_function(g_tsf, strdup(generation), napi_tsfn_nonblocking);
            g_LLMEngineIsFirstToken = false;
        }
        else
        {
            OH_LOG_INFO(LOG_APP, "[INFO] curr generation: %{public}s", generation);
            napi_call_threadsafe_function(g_tsf, strdup(generation), napi_tsfn_nonblocking);
        }
        
        fflush(stdout);
        delete[] generation;
    };
    ret = LLMEngine_Context_SetOnSomeTokenGenerateDoneFunc(llmEngineContext, onSomeTokenGenerateDoneFunc);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_SetOnSomeTokensGenerateDoneFunc failed.");
        return false;
    }

    // 生成失败回调函数
    void(*onGenerateAsyncFailedFunc)(const LLMEngine_Context*) = [](const LLMEngine_Context* ctx)
    {
        (void)ctx;
        g_LLMEngineWaitFlag = false;
        g_LLMEngineGenerateAsyncFailed = true;
    };

    ret = LLMEngine_Context_SetOnGenerateAsyncFailed(llmEngineContext, onGenerateAsyncFailedFunc);

    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_SetOnGenerateAsyncFailedFunc failed.");
        return false;
    }

    g_LLMEngineGenerateAsyncFailed = false;
    g_LLMEngineIsFirstToken = true;
    g_LLMEngineWaitFlag = true;

	// 创建prompt（使用 Qwen ChatML 对话模板格式化）
	std::string formatted = FormatPromptWithChatTemplate(llm_engine_prompt);
	LLMEngine_Prompt* mllmPrompt = LLMEngine_Prompt_Create();

	ret = LLMEngine_Prompt_SetText(mllmPrompt, formatted.c_str());
	if (ret != LLMEngine_SUCCESS)
	{
		OH_LOG_ERROR(LOG_APP, "LLMEngine_Prompt_SetTokenIds failed.");
        return false;
	}

    // 启动异步生成
    ret = LLMEngine_Executor_LLM_GenerateAsync(llmEngineExecutor, llmEngineContext, mllmPrompt);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Executor_LLM_GenerateAsync failed.");
        return false;
    }
	
    while (g_LLMEngineWaitFlag)
    {
        usleep(100000);
    }
    if (g_LLMEngineGenerateAsyncFailed)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Executor_GenerateAsync failed.");
        return false;
    }
    uint32_t len = 0;
    ret = LLMEngine_Context_GetOneGenerationLen(llmEngineContext, &len);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOneGenerationLen failed.");
        return false;
    }
    char* generation = new char [len + 1];
    ret = LLMEngine_Context_GetOneGeneration(llmEngineContext, generation, len);
    if (ret != LLMEngine_SUCCESS)
    {
        OH_LOG_ERROR(LOG_APP, "LLMEngine_Context_GetOneGeneration failed.");
        return false;
    }

    generation[len] = '\0';
    OH_LOG_INFO(LOG_APP, "Check one generation len: %{public}d, generation: %{public}s", len, generation);
    delete[] generation;
	LLMEngine_Prompt_Destroy(&mllmPrompt);
    return true;
}

static int LLMEngineRun(LLMEngineProcessParam& param)
{
    LLMEngine_Status ret;
    OH_LOG_INFO(LOG_APP, "LLM Engine, Run...");

    if(!(param.isAsync))
    {
        if (!(LLMEngineGenerateSync(g_LLMEngineContext, g_LLMEngineExecutor, param.prompt)))
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngineGenerateSync failed");
            return -1;
        }
    }
    else
    {
        if (!(LLMEngineGenerateAsync(g_LLMEngineContext, g_LLMEngineExecutor, param.prompt)))
        {
            OH_LOG_ERROR(LOG_APP, "LLMEngineGenerateAsync failed");
            return -1;
        }
    }
    OH_LOG_INFO(LOG_APP, "LLM Engine, Run Done.");
    return 0;
}

static void LLMEngineDeInit()
{
    OH_LOG_INFO(LOG_APP, "LLM Engine, DeInit...");
    LLMEngine_Executor_Deinit(g_LLMEngineExecutor);
    LLMEngine_Executor_Destroy(&g_LLMEngineExecutor);
    LLMEngine_Context_Destroy(&g_LLMEngineContext);
    OH_LOG_INFO(LOG_APP, "LLM Engine, DeInit Done.");
}

static void CallJsCallback(napi_env env, napi_value jsCb, void *context, void *data)
{
    if (env == nullptr || jsCb == nullptr || data == nullptr)
    {
        OH_LOG_INFO(LOG_APP, "CallJsCallback error");
        return;
    }

    char* token = static_cast<char*>(data);

    OH_LOG_INFO(LOG_APP, "CallJsCallback token is %{public}s", token);

    napi_value js_partial;
    napi_create_string_utf8(env, token, NAPI_AUTO_LENGTH, &js_partial);

    napi_value result;
    napi_call_function(env, nullptr, jsCb, 1, &js_partial, &result);
    
    delete[] token;
}

static napi_value LoadModel(napi_env env, napi_callback_info info)
{
    OH_LOG_INFO(LOG_APP, "LoadModel start");
    std::string loadResult = "";
    int ret;
    napi_value result;
    InitParam();
    ret = LLMEngineInit(param);
    if (ret == 0){
        loadResult = "您好，模型加载完毕，您可以提问了。";
        napi_create_string_utf8(env, loadResult.c_str(), loadResult.size(), &result);
        return result;
    }
    loadResult = "对不起，模型加载失败，请重试。";
    napi_create_string_utf8(env, loadResult.c_str(), loadResult.size(), &result);
    return result;
}

static napi_value AnswerGet(napi_env env, napi_callback_info info)
{
    OH_LOG_INFO(LOG_APP, "AnswerGet start");
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "Thread-safe Function", NAPI_AUTO_LENGTH, &resourceName);

    napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, nullptr, 
        CallJsCallback, &g_tsf);
    
    return nullptr;
}

static napi_value ModelInfer(napi_env env, napi_callback_info info) {
    OH_LOG_INFO(LOG_APP, "ModelInfer start");
    
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    auto callbackData = new CallbackData();
    size_t strLength;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &strLength);
    char* strBuffer = new char[strLength + 1];
    napi_get_value_string_utf8(env, args[0], strBuffer, strLength + 1, &strLength);
    callbackData->question = strBuffer;
    delete [] strBuffer;
    param.prompt = callbackData->question;
    std::thread(LLMEngineRun, std::ref(param)).detach();
    delete callbackData;
    return nullptr;
}

static napi_value deInitModel(napi_env env, napi_callback_info info) {
    OH_LOG_INFO(LOG_APP, "deInitModel start");
    LLMEngineDeInit();
    return nullptr;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "loadmodel", nullptr, LoadModel, nullptr, nullptr, nullptr, napi_default, nullptr},
        { "modelinfer", nullptr, ModelInfer, nullptr, nullptr, nullptr, napi_default, nullptr},
        { "answerget", nullptr, AnswerGet, nullptr, nullptr, nullptr, napi_default, nullptr},
        { "deinitmodel", nullptr, deInitModel, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
