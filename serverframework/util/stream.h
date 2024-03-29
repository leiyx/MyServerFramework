/**
 * @file stream.h
 * @brief 流接口
 */

#ifndef STREAM_H
#define STREAM_H

#include <memory>

#include "util/bytearray.h"

namespace serverframework {

/**
 * @brief 流结构
 */
class Stream {
 public:
  using ptr = std::shared_ptr<Stream>;
  /**
   * @brief 析构函数
   */
  virtual ~Stream() {}

  /**
   * @brief 读数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int read(void* buffer, size_t length) = 0;

  /**
   * @brief 读数据
   * @param[out] ba 接收数据的ByteArray
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int read(ByteArray::ptr ba, size_t length) = 0;

  /**
   * @brief 读固定长度的数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int ReadFixSize(void* buffer, size_t length);

  /**
   * @brief 读固定长度的数据
   * @param[out] ba 接收数据的ByteArray
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int ReadFixSize(ByteArray::ptr ba, size_t length);

  /**
   * @brief 写数据
   * @param[in] buffer 写数据的内存
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int write(const void* buffer, size_t length) = 0;

  /**
   * @brief 写数据
   * @param[in] ba 写数据的ByteArray
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int write(ByteArray::ptr ba, size_t length) = 0;

  /**
   * @brief 写固定长度的数据
   * @param[in] buffer 写数据的内存
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int WriteFixSize(const void* buffer, size_t length);

  /**
   * @brief 写固定长度的数据
   * @param[in] ba 写数据的ByteArray
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual int WriteFixSize(ByteArray::ptr ba, size_t length);

  /**
   * @brief 关闭流
   */
  virtual void close() = 0;
};

}  // namespace serverframework

#endif
