#include "Logger.h"
#include "AssetPath.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <iostream>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "DbgHelp.lib")

LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    // クラッシュとみなす主要な例外コード
    if (code == EXCEPTION_ACCESS_VIOLATION || 
        code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        code == EXCEPTION_DATATYPE_MISALIGNMENT ||
        code == EXCEPTION_FLT_DENORMAL_OPERAND ||
        code == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_FLT_INEXACT_RESULT ||
        code == EXCEPTION_FLT_INVALID_OPERATION ||
        code == EXCEPTION_FLT_OVERFLOW ||
        code == EXCEPTION_FLT_STACK_CHECK ||
        code == EXCEPTION_FLT_UNDERFLOW ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_IN_PAGE_ERROR ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_INT_OVERFLOW ||
        code == EXCEPTION_INVALID_DISPOSITION ||
        code == EXCEPTION_NONCONTINUABLE_EXCEPTION ||
        code == EXCEPTION_PRIV_INSTRUCTION ||
        code == EXCEPTION_STACK_OVERFLOW) 
    {
        std::stringstream ss;
        ss << "例外がスローされました: 深刻なエラー (Exception Code: 0x" << std::hex << std::uppercase << code << ")\n";
        ss << "🚨 0x" << pExceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
        ss << "--- Stack Trace ---\n";

        void* stack[64];
        WORD numFrames = CaptureStackBackTrace(0, 64, stack, NULL);
        HANDLE process = GetCurrentProcess();

        // 根本原因の特定 (RIP/EIP レジスタから直接アドレスを取得)
        DWORD64 faultingAddress = 0;
#ifdef _M_X64
        faultingAddress = pExceptionInfo->ContextRecord->Rip;
#elif defined(_M_IX86)
        faultingAddress = pExceptionInfo->ContextRecord->Eip;
#endif
        std::string rootCauseFile = "";
        DWORD rootCauseLineNum = 0;
        DWORD rootDisp = 0;
        IMAGEHLP_LINE64 rootLine;
        ZeroMemory(&rootLine, sizeof(rootLine));
        rootLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        if (faultingAddress != 0 && SymGetLineFromAddr64(process, faultingAddress, &rootDisp, &rootLine)) {
            rootCauseFile = rootLine.FileName;
            rootCauseLineNum = rootLine.LineNumber;
        } else {
            // フォールバック: スタックからLogger以外の最初の関数を探す
            for (WORD i = 0; i < numFrames; ++i) {
                if (SymGetLineFromAddr64(process, (DWORD64)stack[i], &rootDisp, &rootLine)) {
                    std::string fName = rootLine.FileName;
                    if (fName.find("Logger.cpp") == std::string::npos) {
                        rootCauseFile = fName;
                        rootCauseLineNum = rootLine.LineNumber;
                        break;
                    }
                }
            }
        }

        for (WORD i = 0; i < numFrames; ++i) {
            DWORD64 displacement = 0;
            IMAGEHLP_LINE64 line;
            ZeroMemory(&line, sizeof(line));
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;

            std::string funcName = "UnknownFunction";
            if (SymFromAddr(process, (DWORD64)stack[i], &displacement, pSymbol)) {
                funcName = pSymbol->Name;
            }

            DWORD displacementLine = 0;
            if (SymGetLineFromAddr64(process, (DWORD64)stack[i], &displacementLine, &line)) {
                ss << "    " << funcName << " (" << line.FileName << " line " << std::dec << line.LineNumber << ")\n";
            } else {
                ss << "    " << funcName << " (Unknown line)\n";
            }
        }
        ss << "-------------------\n";

        if (!rootCauseFile.empty()) {
            ss << "🎯 根本原因: " << rootCauseFile << " line " << std::dec << rootCauseLineNum << "\n";
        }

        Logger::GetInstance().Log("%s", ss.str().c_str());
        Logger::GetInstance().Finalize(); // ファイルを確実に保存する
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void Logger::Initialize() {
    // ログは exe と同じ階層の logs フォルダに出す。
    // exe からの相対位置だけで決めるので、配布先のフォルダ構成(深さ・日本語名)に依存しない。
    // 以前は「exe の4つ上」を決め打ちしていたため、配布先では書き込み禁止の場所を指して落ちていた。
    const std::filesystem::path logDir = AbsoluteEngine::AssetPath::GetExeDirectory() / "logs";

    // 例外を投げない版を使う。作れない場合(書き込み禁止の場所など)はログ無しで続行する
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &in_time_t);
    std::wstringstream ss;
    ss << std::put_time(&tm_buf, L"%Y%m%d_%H%M%S") << L".log";

    // path のまま開く(string 経由にすると日本語フォルダで開けなくなる)
    if (!ec) {
        logFile_.open(logDir / ss.str(), std::ios::out | std::ios::trunc);
    }
    
    if (logFile_.is_open()) {
        Log("[INFO] Logger Initialized.");
    }

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    exceptionHandlerHandle_ = AddVectoredExceptionHandler(1, VectoredExceptionHandler);
}

void Logger::Finalize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (exceptionHandlerHandle_) {
        RemoveVectoredExceptionHandler(exceptionHandlerHandle_);
        exceptionHandlerHandle_ = nullptr;
    }
    SymCleanup(GetCurrentProcess());
    if (logFile_.is_open()) {
        logFile_.flush();
        logFile_.close();
    }
}

void Logger::Log(const char* format, ...) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    va_list args;
    va_start(args, format);
    int size = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (size <= 0) return;

    std::vector<char> buffer(size + 1);
    va_start(args, format);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);

    std::string message(buffer.data());
    if (!message.empty() && message.back() != '\n') {
        message += '\n';
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), (int)message.size(), NULL, 0);
    if (size_needed > 0) {
        std::wstring wmessage(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, message.c_str(), (int)message.size(), &wmessage[0], size_needed);
        OutputDebugStringW(wmessage.c_str());
    }

    if (logFile_.is_open()) {
        logFile_ << message;
        logFile_.flush(); // リアルタイムで書き込むために毎回flush
    }
}

void Logger::CheckDirectXErrors(ID3D12Device* device) {
#ifdef _DEBUG
    if (!device) return;

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        UINT64 numMessages = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < numMessages; ++i) {
            SIZE_T messageLength = 0;
            infoQueue->GetMessage(i, nullptr, &messageLength);
            
            if (messageLength > 0) {
                D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageLength);
                if (infoQueue->GetMessage(i, message, &messageLength) == S_OK) {
                    if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || 
                        message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) {
                        Log("D3D12 ERROR: %s", message->pDescription);
                    } else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
                        Log("D3D12 WARNING: %s", message->pDescription);
                    }
                }
                free(message);
            }
        }
        if (numMessages > 0) {
            infoQueue->ClearStoredMessages();
        }
    }
#endif
}
