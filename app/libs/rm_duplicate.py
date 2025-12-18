import os


logs = """
Duplicate class com.google.common.util.concurrent.ListenableFuture found in modules listenablefuture-1.0.jar -> listenablefuture-1.0 (com.google.guava:listenablefuture:1.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.CanIgnoreReturnValue found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.CheckReturnValue found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.CompatibleWith found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.CompileTimeConstant found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.DoNotCall found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.DoNotMock found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.ForOverride found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.FormatMethod found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.FormatString found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.Immutable found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.IncompatibleModifiers found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.InlineMe found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.InlineMeValidationDisabled found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.Keep found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.Modifier found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.MustBeClosed found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.NoAllocation found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.OverridingMethodsMustInvokeSuper found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.RequiredModifiers found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.RestrictedApi found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.SuppressPackageLocation found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.Var found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.concurrent.GuardedBy found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.concurrent.LazyInit found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.concurrent.LockMethod found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
Duplicate class com.google.errorprone.annotations.concurrent.UnlockMethod found in modules error_prone_annotations-2.15.0.jar -> error_prone_annotations-2.15.0 (com.google.errorprone:error_prone_annotations:2.15.0) and lspatch_cleaned.jar -> lspatch_cleaned (lspatch_cleaned.jar)
"""

for i in logs.split("\n"):
    if i.startswith("Duplicate class "):
        flag_pos = i.find(" found in modules")
        if flag_pos == -1:
            continue
        class_name = i[len("Duplicate class "):flag_pos]
        file_name = class_name.replace(".", "/") + ".class"
        if not os.path.isfile(file_name):
            print(file_name, "not found!!!")
        else:
            os.remove(file_name)
            print(file_name, "removed.")
