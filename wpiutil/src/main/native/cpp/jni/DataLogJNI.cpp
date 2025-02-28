// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <jni.h>

#include <fmt/format.h>

#include "WPIUtilJNI.h"
#include "edu_wpi_first_util_datalog_DataLogJNI.h"
#include "wpi/DataLog.h"
#include "wpi/jni_util.h"

using namespace wpi::java;
using namespace wpi::log;

extern "C" {

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    create
 * Signature: (Ljava/lang/String;Ljava/lang/String;DLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_create
  (JNIEnv* env, jclass, jstring dir, jstring filename, jdouble period,
   jstring extraHeader)
{
  if (!dir) {
    wpi::ThrowNullPointerException(env, "dir is null");
    return 0;
  }
  if (!filename) {
    wpi::ThrowNullPointerException(env, "filename is null");
    return 0;
  }
  if (!extraHeader) {
    wpi::ThrowNullPointerException(env, "extraHeader is null");
    return 0;
  }
  return reinterpret_cast<jlong>(new DataLog{JStringRef{env, dir},
                                             JStringRef{env, filename}, period,
                                             JStringRef{env, extraHeader}});
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    setFilename
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_setFilename
  (JNIEnv* env, jclass, jlong impl, jstring filename)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!filename) {
    wpi::ThrowNullPointerException(env, "filename is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->SetFilename(JStringRef{env, filename});
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    flush
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_flush
  (JNIEnv* env, jclass, jlong impl)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->Flush();
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    pause
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_pause
  (JNIEnv* env, jclass, jlong impl)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->Pause();
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    resume
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_resume
  (JNIEnv* env, jclass, jlong impl)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->Resume();
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    start
 * Signature: (JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;J)I
 */
JNIEXPORT jint JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_start
  (JNIEnv* env, jclass, jlong impl, jstring name, jstring type,
   jstring metadata, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return 0;
  }
  return reinterpret_cast<DataLog*>(impl)->Start(
      JStringRef{env, name}, JStringRef{env, type}, JStringRef{env, metadata},
      timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    finish
 * Signature: (JIJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_finish
  (JNIEnv* env, jclass, jlong impl, jint entry, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->Finish(entry, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    setMetadata
 * Signature: (JILjava/lang/String;J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_setMetadata
  (JNIEnv* env, jclass, jlong impl, jint entry, jstring metadata,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->SetMetadata(
      entry, JStringRef{env, metadata}, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    close
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_close
  (JNIEnv*, jclass, jlong impl)
{
  delete reinterpret_cast<DataLog*>(impl);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendRaw
 * Signature: (JI[BIIJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendRaw
  (JNIEnv* env, jclass, jlong impl, jint entry, jbyteArray value, jint start,
   jint length, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  if (start < 0) {
    wpi::ThrowIndexOobException(env, "start must be >= 0");
    return;
  }
  if (length < 0) {
    wpi::ThrowIndexOobException(env, "length must be >= 0");
    return;
  }
  CriticalJSpan<const jbyte> cvalue{env, value};
  if (static_cast<unsigned int>(start + length) > cvalue.size()) {
    wpi::ThrowIndexOobException(
        env, "start + len must be smaller than array length");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendRaw(
      entry, cvalue.uarray().subspan(start, length), timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendRawBuffer
 * Signature: (JILjava/lang/Object;IIJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendRawBuffer
  (JNIEnv* env, jclass, jlong impl, jint entry, jobject value, jint start,
   jint length, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  if (start < 0) {
    wpi::ThrowIndexOobException(env, "start must be >= 0");
    return;
  }
  if (length < 0) {
    wpi::ThrowIndexOobException(env, "length must be >= 0");
    return;
  }
  JSpan<const jbyte> cvalue{env, value, static_cast<size_t>(start + length)};
  if (!cvalue) {
    wpi::ThrowIllegalArgumentException(env,
                                       "value must be a native ByteBuffer");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendRaw(
      entry, cvalue.uarray().subspan(start, length), timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendBoolean
 * Signature: (JIZJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendBoolean
  (JNIEnv* env, jclass, jlong impl, jint entry, jboolean value, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendBoolean(entry, value, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendInteger
 * Signature: (JIJJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendInteger
  (JNIEnv* env, jclass, jlong impl, jint entry, jlong value, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendInteger(entry, value, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendFloat
 * Signature: (JIFJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendFloat
  (JNIEnv* env, jclass, jlong impl, jint entry, jfloat value, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendFloat(entry, value, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendDouble
 * Signature: (JIDJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendDouble
  (JNIEnv* env, jclass, jlong impl, jint entry, jdouble value, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendDouble(entry, value, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendString
 * Signature: (JILjava/lang/String;J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendString
  (JNIEnv* env, jclass, jlong impl, jint entry, jstring value, jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendString(entry, JStringRef{env, value},
                                                 timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendBooleanArray
 * Signature: (JI[ZJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendBooleanArray
  (JNIEnv* env, jclass, jlong impl, jint entry, jbooleanArray value,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendBooleanArray(
      entry, JSpan<const jboolean>{env, value}, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendIntegerArray
 * Signature: (JI[JJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendIntegerArray
  (JNIEnv* env, jclass, jlong impl, jint entry, jlongArray value,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  JSpan<const jlong> jarr{env, value};
  if constexpr (sizeof(jlong) == sizeof(int64_t)) {
    reinterpret_cast<DataLog*>(impl)->AppendIntegerArray(
        entry, {reinterpret_cast<const int64_t*>(jarr.data()), jarr.size()},
        timestamp);
  } else {
    wpi::SmallVector<int64_t, 16> arr;
    arr.reserve(jarr.size());
    for (auto v : jarr) {
      arr.push_back(v);
    }
    reinterpret_cast<DataLog*>(impl)->AppendIntegerArray(entry, arr, timestamp);
  }
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendFloatArray
 * Signature: (JI[FJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendFloatArray
  (JNIEnv* env, jclass, jlong impl, jint entry, jfloatArray value,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendFloatArray(
      entry, JSpan<const jfloat>{env, value}, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendDoubleArray
 * Signature: (JI[DJ)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendDoubleArray
  (JNIEnv* env, jclass, jlong impl, jint entry, jdoubleArray value,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  reinterpret_cast<DataLog*>(impl)->AppendDoubleArray(
      entry, JSpan<const jdouble>{env, value}, timestamp);
}

/*
 * Class:     edu_wpi_first_util_datalog_DataLogJNI
 * Method:    appendStringArray
 * Signature: (JI[Ljava/lang/Object;J)V
 */
JNIEXPORT void JNICALL
Java_edu_wpi_first_util_datalog_DataLogJNI_appendStringArray
  (JNIEnv* env, jclass, jlong impl, jint entry, jobjectArray value,
   jlong timestamp)
{
  if (impl == 0) {
    wpi::ThrowNullPointerException(env, "impl is null");
    return;
  }
  if (!value) {
    wpi::ThrowNullPointerException(env, "value is null");
    return;
  }
  size_t len = env->GetArrayLength(value);
  std::vector<std::string> arr;
  arr.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    JLocal<jstring> elem{
        env, static_cast<jstring>(env->GetObjectArrayElement(value, i))};
    if (!elem) {
      wpi::ThrowNullPointerException(
          env, fmt::format("string at element {} is null", i));
      return;
    }
    arr.emplace_back(JStringRef{env, elem}.str());
  }
  reinterpret_cast<DataLog*>(impl)->AppendStringArray(entry, arr, timestamp);
}

}  // extern "C"
