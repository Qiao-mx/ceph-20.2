/**
 * @file rgw_role_serde.h
 * @brief 角色管理序列化接口
 *
 * IAM 角色信息的序列化与反序列化接口。
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 角色类型定义
 *============================================================================*/

/**
 * @brief 角色权限策略条目
 */
typedef struct {
    char* policy_name;          /**< 策略名称 */
    char* policy_doc;           /**< 策略文档 (JSON) */
} rgw_role_policy_t;

/**
 * @brief 角色标签条目
 */
typedef struct {
    char* key;                  /**< 标签键 */
    char* value;                /**< 标签值 */
} rgw_role_tag_t;

/**
 * @brief 角色信息 (序列化格式)
 *
 * 对应 C++ 中的 RGWRoleInfo。
 */
typedef struct {
    char* id;                   /**< 角色 ID (UUID) */
    char* name;                 /**< 角色名称 */
    char* path;                 /**< 路径 */
    char* arn;                  /**< ARN */
    char* creation_date;        /**< 创建日期 (ISO 8601) */
    char* trust_policy;          /**< 信任策略 (JSON) */
    char* description;          /**< 描述 */
    uint64_t max_session_duration; /**< 最大会话时长 (秒) */
    char* tenant;                /**< 租户 */
    char* account_id;           /**< 账户 ID */

    /* 权限策略映射 (map<string, string>) */
    rgw_role_policy_t* policies;    /**< 策略数组 */
    size_t policies_count;          /**< 策略数量 */

    /* 托管策略 ARN 列表 */
    char** managed_policies;         /**< 托管策略 ARN 数组 */
    size_t managed_policies_count;  /**< 托管策略数量 */

    /* 标签 (multimap<string, string>) */
    rgw_role_tag_t* tags;           /**< 标签数组 */
    size_t tags_count;              /**< 标签数量 */

    /* 版本控制 */
    uint32_t ver;                    /**< 版本号 */
    time_t mtime;                   /**< 修改时间 */
} rgw_role_t;

/*============================================================================
 * 函数声明
 *============================================================================*/

/**
 * @brief 创建角色信息
 *
 * @return 新创建的角色信息，失败返回 NULL
 */
rgw_role_t* rgw_role_create(void);

/**
 * @brief 销毁角色信息
 *
 * 释放角色占用的所有资源。
 *
 * @param role 要销毁的角色
 */
void rgw_role_destroy(rgw_role_t* role);

/**
 * @brief 初始化角色信息
 *
 * @param role 角色
 * @param id 角色 ID
 * @param name 角色名称
 * @param tenant 租户
 * @param account_id 账户 ID
 *
 * @return 执行结果
 */
int rgw_role_init(rgw_role_t* role,
                  const char* id,
                  const char* name,
                  const char* tenant,
                  const char* account_id);

/**
 * @brief 克隆角色信息
 *
 * @param src 源角色
 *
 * @return 克隆的角色，失败返回 NULL
 */
rgw_role_t* rgw_role_clone(const rgw_role_t* src);

/**
 * @brief 释放角色成员
 *
 * 释放角色内部动态分配的成员，但保留结构体本身。
 *
 * @param role 角色
 */
void rgw_role_free_members(rgw_role_t* role);

/*============================================================================
 * 策略管理
 *============================================================================*/

/**
 * @brief 添加角色策略
 *
 * @param role 角色
 * @param policy_name 策略名称
 * @param policy_doc 策略文档
 *
 * @return 执行结果
 */
int rgw_role_add_policy(rgw_role_t* role,
                        const char* policy_name,
                        const char* policy_doc);

/**
 * @brief 获取角色策略
 *
 * @param role 角色
 * @param policy_name 策略名称
 * @param policy_doc 输出：策略文档
 *
 * @return 执行结果，找不到返回 RGW_ERR_NOT_FOUND
 */
int rgw_role_get_policy(const rgw_role_t* role,
                        const char* policy_name,
                        char** policy_doc);

/**
 * @brief 删除角色策略
 *
 * @param role 角色
 * @param policy_name 策略名称
 *
 * @return 执行结果
 */
int rgw_role_delete_policy(rgw_role_t* role, const char* policy_name);

/**
 * @brief 获取所有策略名称
 *
 * @param role 角色
 * @param policy_names 输出：策略名称数组
 * @param count 输出：策略数量
 *
 * @return 执行结果
 */
int rgw_role_list_policies(const rgw_role_t* role,
                           char*** policy_names,
                           size_t* count);

/*============================================================================
 * 标签管理
 *============================================================================*/

/**
 * @brief 添加角色标签
 *
 * @param role 角色
 * @param key 标签键
 * @param value 标签值
 *
 * @return 执行结果
 */
int rgw_role_add_tag(rgw_role_t* role,
                     const char* key,
                     const char* value);

/**
 * @brief 获取角色标签
 *
 * @param role 角色
 * @param tags 输出：标签数组
 * @param count 输出：标签数量
 *
 * @return 执行结果
 */
int rgw_role_get_tags(const rgw_role_t* role,
                      rgw_role_tag_t** tags,
                      size_t* count);

/**
 * @brief 删除角色标签
 *
 * @param role 角色
 * @param keys 要删除的标签键数组
 * @param keys_count 标签键数量
 *
 * @return 执行结果
 */
int rgw_role_delete_tags(rgw_role_t* role,
                         const char** keys,
                         size_t keys_count);

/*============================================================================
 * 序列化/反序列化
 *============================================================================*/

/**
 * @brief 计算角色编码大小
 *
 * @param role 角色
 *
 * @return 需要的缓冲区大小
 */
size_t rgw_role_calc_encode_size(const rgw_role_t* role);

/**
 * @brief 编码角色信息
 *
 * @param role 角色
 * @param buf 输出缓冲区（可以为 NULL，用于计算大小）
 * @param buf_size 缓冲区大小
 * @param actual_size 实际编码大小（输出）
 *
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_role_encode(const rgw_role_t* role,
                    uint8_t* buf,
                    size_t buf_size,
                    size_t* actual_size);

/**
 * @brief 解码角色信息
 *
 * @param buf 输入缓冲区
 * @param buf_size 缓冲区大小
 * @param role 输出角色
 *
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_role_decode(const uint8_t* buf,
                    size_t buf_size,
                    rgw_role_t* role);

/**
 * @brief 分配并编码角色信息
 *
 * @param role 角色
 * @param buf_out 输出缓冲区（需要调用者 free）
 * @param buf_len_out 输出缓冲区长度
 *
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_role_encode_alloc(const rgw_role_t* role,
                          uint8_t** buf_out,
                          size_t* buf_len_out);

/*============================================================================
 * ARN 生成
 *============================================================================*/

/**
 * @brief 生成角色 ARN
 *
 * ARN 格式: arn:aws:iam::{account_id}:role/{path}{role-name}
 *
 * @param role 角色
 *
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_role_generate_arn(rgw_role_t* role);

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * @brief 创建角色策略条目
 *
 * @param policy_name 策略名称
 * @param policy_doc 策略文档
 *
 * @return 新创建的角色策略，失败返回 NULL
 */
rgw_role_policy_t* rgw_role_policy_create(const char* policy_name,
                                          const char* policy_doc);

/**
 * @brief 销毁角色策略条目
 *
 * @param policy 策略
 */
void rgw_role_policy_destroy(rgw_role_policy_t* policy);

/**
 * @brief 创建角色标签条目
 *
 * @param key 标签键
 * @param value 标签值
 *
 * @return 新创建的角色标签，失败返回 NULL
 */
rgw_role_tag_t* rgw_role_tag_create(const char* key, const char* value);

/**
 * @brief 销毁角色标签条目
 *
 * @param tag 标签
 */
void rgw_role_tag_destroy(rgw_role_tag_t* tag);

#ifdef __cplusplus
}
#endif
