#pragma once

#include "art/runtime/art_method.hpp"
#include "art/runtime/obj_ptr.hpp"
#include "art/runtime/thread.hpp"
#include "common.hpp"

namespace lsplant::art {
class ClassLinker {
private:
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, SetEntryPointsToInterpreter, ClassLinker *thiz,
                                 ArtMethod *art_method) {
        if (SetEntryPointsToInterpreterSym) [[likely]] {
            SetEntryPointsToInterpreterSym(thiz, art_method);
        }
    }

    CREATE_HOOK_STUB_ENTRY(
            "_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv", bool,
            ShouldUseInterpreterEntrypoint, (ArtMethod * art_method, const void *quick_code), {
                if (quick_code != nullptr && IsHooked(art_method)) [[unlikely]] {
                    return false;
                }
                return backup(art_method, quick_code);
            });


    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_to_interpreter_bridge, void *) {}

    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_generic_jni_trampoline, void *) {}

    inline static art::ArtMethod *MayGetBackup(art::ArtMethod *method) {
        if (auto backup = IsHooked(method); backup) [[unlikely]] {
            method = backup;
            LOGV("propagate native method: %s", method->PrettyMethod(true).data());
        }
        return method;
    }

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art6mirror9ArtMethod14RegisterNativeEPNS_6ThreadEPKvb", void, RegisterNativeThread,
        (art::ArtMethod * method, art::Thread *thread, const void *native_method, bool is_fast),
        { return backup(MayGetBackup(method), thread, native_method, is_fast); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art6mirror9ArtMethod16UnregisterNativeEPNS_6ThreadE", void,
                               UnregisterNativeThread,
                               (art::ArtMethod * method, art::Thread *thread),
                               { return backup(MayGetBackup(method), thread); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art9ArtMethod14RegisterNativeEPKvb", void, RegisterNativeFast,
                               (art::ArtMethod * method, const void *native_method, bool is_fast),
                               { return backup(MayGetBackup(method), native_method, is_fast); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art9ArtMethod16UnregisterNativeEv", void, UnregisterNativeFast,
                               (art::ArtMethod * method), { return backup(MayGetBackup(method)); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art9ArtMethod14RegisterNativeEPKv", const void *,
                               RegisterNative, (art::ArtMethod * method, const void *native_method),
                               { return backup(MayGetBackup(method), native_method); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art9ArtMethod16UnregisterNativeEv", const void *,
                               UnregisterNative, (art::ArtMethod * method),
                               { return backup(MayGetBackup(method)); });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker14RegisterNativeEPNS_6ThreadEPNS_9ArtMethodEPKv", const void *,
        RegisterNativeClassLinker,
        (art::ClassLinker * thiz, art::Thread *self, art::ArtMethod *method,
         const void *native_method),
        { return backup(thiz, self, MayGetBackup(method), native_method); });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art11ClassLinker16UnregisterNativeEPNS_6ThreadEPNS_9ArtMethodE",
                               const void *, UnregisterNativeClassLinker,
                               (art::ClassLinker * thiz, art::Thread *self, art::ArtMethod *method),
                               { return backup(thiz, self, MayGetBackup(method)); });

    static auto RestoreBackup(const dex::ClassDef *class_def, art::Thread *self) {
        auto methods = mirror::Class::PopBackup(class_def, self);
        for (const auto &[art_method, old_trampoline] : methods) {
            auto new_trampoline = art_method->GetEntryPoint();
            art_method->SetEntryPoint(old_trampoline);
            auto deoptimized = IsDeoptimized(art_method);
            auto backup_method = IsHooked(art_method);
            if (backup_method) {
                // If deoptimized, the backup entrypoint should be already set to interpreter
                if (!deoptimized && new_trampoline != old_trampoline) [[unlikely]] {
                    LOGV("propagate entrypoint for orig %p backup %p", art_method, backup_method);
                    backup_method->SetEntryPoint(new_trampoline);
                }
            } else if (deoptimized) {
                if (new_trampoline != art_quick_to_interpreter_bridge &&
                    new_trampoline != art_quick_generic_jni_trampoline) {
                    LOGV("re-deoptimize for %p", art_method);
                    SetEntryPointsToInterpreter(art_method);
                }
            }
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE", void,
        FixupStaticTrampolines, (ClassLinker * thiz, ObjPtr<mirror::Class> mirror_class), {
            backup(thiz, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), nullptr);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE",
        void, FixupStaticTrampolinesWithThread,
        (ClassLinker * thiz, art::Thread *self, ObjPtr<mirror::Class> mirror_class), {
            backup(thiz, self, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), self);
        });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6mirror5ClassE",
                               void, FixupStaticTrampolinesRaw,
                               (ClassLinker * thiz, mirror::Class *mirror_class), {
                                   backup(thiz, mirror_class);
                                   RestoreBackup(mirror_class->GetClassDef(), nullptr);
                               });

    CREATE_MEM_HOOK_STUB_ENTRY(
        LP_SELECT(
            "_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEi",
            "_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEl"),
        void, AdjustThreadVisibilityCounter, (void *thiz, art::Thread *self, ssize_t adjustment), {
            backup(thiz, self, adjustment);
            RestoreBackup(nullptr, self);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker26VisiblyInitializedCallback22MarkVisiblyInitializedEPNS_6ThreadE",
        void, MarkVisiblyInitialized, (void *thiz, Thread* self), {
            backup(thiz, self);
            RestoreBackup(nullptr, self);
        });
public:
    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_N__ && sdk_int < __ANDROID_API_T__) {
            HookSyms(handler, ShouldUseInterpreterEntrypoint);
        }

        if (!HookSyms(handler, FixupStaticTrampolinesWithThread, FixupStaticTrampolines,
                      FixupStaticTrampolinesRaw)) {
            return false;
        }

        if (!HookSyms(handler, RegisterNativeClassLinker, RegisterNative, RegisterNativeFast,
                      RegisterNativeThread) ||
            !HookSyms(handler, UnregisterNativeClassLinker, UnregisterNative, UnregisterNativeFast,
                      UnregisterNativeThread)) {
            return false;
        }

        if (sdk_int >= __ANDROID_API_R__) {
            if constexpr (GetArch() != Arch::kX86 && GetArch() != Arch::kX86_64) {
                // fixup static trampoline may have been inlined
                HookSyms(handler, AdjustThreadVisibilityCounter, MarkVisiblyInitialized);
            }
        }

        if (!RETRIEVE_MEM_FUNC_SYMBOL(
                SetEntryPointsToInterpreter,
                "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE"))
            [[unlikely]] {
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_to_interpreter_bridge,
                                      "art_quick_to_interpreter_bridge")) [[unlikely]] {
                return false;
            }
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_generic_jni_trampoline,
                                      "art_quick_generic_jni_trampoline")) [[unlikely]] {
                return false;
            }
            LOGD("art_quick_to_interpreter_bridge = %p", art_quick_to_interpreter_bridgeSym);
            LOGD("art_quick_generic_jni_trampoline = %p", art_quick_generic_jni_trampolineSym);
        }
        return true;
    }

    [[gnu::always_inline]] static bool SetEntryPointsToInterpreter(ArtMethod *art_method) {
        if (SetEntryPointsToInterpreterSym) [[likely]] {
            SetEntryPointsToInterpreter(nullptr, art_method);
            return true;
        }
        // Android 13
        if (art_quick_to_interpreter_bridgeSym && art_quick_generic_jni_trampolineSym) [[likely]] {
            if (art_method->GetAccessFlags() & ArtMethod::kAccNative) [[unlikely]] {
                LOGV("deoptimize native method %s from %p to %p",
                     art_method->PrettyMethod(true).data(), art_method->GetEntryPoint(),
                     art_quick_generic_jni_trampolineSym);
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(art_quick_generic_jni_trampolineSym));
            } else {
                LOGV("deoptimize method %s from %p to %p", art_method->PrettyMethod(true).data(),
                     art_method->GetEntryPoint(), art_quick_to_interpreter_bridgeSym);
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(art_quick_to_interpreter_bridgeSym));
            }
            return true;
        }
        return false;
    }
};
}  // namespace lsplant::art
