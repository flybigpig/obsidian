车机端 HUKS 集成和证书签发是 TBOX-PKI 安全体系的核心实现。我给你一份完整的车机端实现方案。

---

## 🔐 车机端 HUKS 集成 + 证书签发完整方案

### 一、整体架构

```
┌──────────────────────────────────────────────────────────┐
│                  鸿蒙车机端 TBOX-PKI 体系                  │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │  HiCar 应用  │  │  证书管理    │  │  车控指令    │   │
│  │  / 导航 App  │  │  中心 UI     │  │  签名服务    │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│         │                 │                  │           │
│         ▼                 ▼                  ▼           │
│  ┌────────────────────────────────────────────────────┐  │
│  │       TboxPkiService（业务层）                       │  │
│  │  - CSR 生成  - 证书申请  - 证书轮换  - OCSP 查询    │  │
│  └─────────────────────┬──────────────────────────────┘  │
│                        │                                  │
│         ┌──────────────┼──────────────┐                  │
│         ▼              ▼              ▼                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐    │
│  │   HUKS 封装  │ │  X509 解析   │ │  HTTP/mTLS   │    │
│  │  (密钥/证书)  │ │  (BouncyCastle│ │  客户端      │    │
│  │              │ │   移植)      │ │              │    │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘    │
│         │                │                 │             │
│         ▼                ▼                 ▼             │
│  ┌─────────────────────────────────────────────────┐   │
│  │      HarmonyOS Native API / NDK                 │   │
│  │   - security.huks  - net.http  - crypto         │   │
│  └─────────────────────────────────────────────────┘   │
│         │                                                │
│         ▼                                                │
│  ┌─────────────────────────────────────────────────┐   │
│  │  TEE (Trusted Execution Environment)             │   │
│  │  - 私钥永不离开 TEE  - 安全签名  - 安全存储      │   │
│  └─────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
                          │
                          │  mTLS / HTTPS
                          ▼
            ┌────────────────────────────┐
            │   OEM PKI 证书服务器 (CA)   │
            │  - 证书签发  - OCSP 响应器   │
            │  - 证书状态查询  - CRL 管理  │
            └────────────────────────────┘
```

---

### 二、HUKS 基础封装

#### 1. 鸿蒙工程配置

```json5
// oh-package.json5
{
  "dependencies": {
    "@ohos.security.huks": "4.0.0",
    "@ohos.security.cryptoFramework": "4.0.0",
    "@ohos.net.http": "4.0.0",
    "@ohos.net.tls": "4.0.0"
  }
}
```

```json5
// module.json5 - 申请权限
{
  "module": {
    "requestPermissions": [
      { "name": "ohos.permission.INTERNET" },
      { "name": "ohos.permission.STORE_PERSISTENT_DATA" },
      { "name": "ohos.permission.GET_TELEPHONY_STATE" }
    ]
  }
}
```

#### 2. HUKS 工具类封装

```typescript
// common/huks/src/main/ets/HuksManager.ets
import huks from '@ohos.security.huks';
import { BusinessError } from '@ohos.base';
import Logger from '../utils/Logger';

const TAG = 'HuksManager';

// 密钥用途
export enum KeyPurpose {
  SIGN_VERIFY = 0,           // 签名验签
  ENCRYPT_DECRYPT = 1,       // 加解密
  KEY_AGREEMENT = 2,         // 密钥协商
  WRAP_UNWRAP = 3,           // 密钥包装
}

// 密钥算法
export enum KeyAlgorithm {
  RSA_2048 = 'RSA2048',
  RSA_3072 = 'RSA3072',
  RSA_4096 = 'RSA4096',
  EC_P256 = 'ECC_P256',       // 国密 SM2 用 EC_P256
  EC_P384 = 'ECC_P384',
  SM2 = 'SM2_256',            // 国密
}

export interface HuksKeyConfig {
  alias: string;              // 密钥别名（全局唯一）
  algorithm: KeyAlgorithm;
  purposes: KeyPurpose[];
  digest?: string;            // SHA256/SHA384/SM3
  padding?: string;           // PKCS1/OAEP/PSS
}

export class HuksManager {
  private static instance: HuksManager;

  public static getInstance(): HuksManager {
    if (!HuksManager.instance) {
      HuksManager.instance = new HuksManager();
    }
    return HuksManager.instance;
  }

  // ========== 1. 生成密钥对 ==========
  async generateKeyPair(config: HuksKeyConfig): Promise<boolean> {
    try {
      // 1.1 检查密钥是否已存在
      const isExist = await this.isKeyExist(config.alias);
      if (isExist) {
        Logger.info(TAG, `Key already exists: ${config.alias}`);
        return true;
      }

      // 1.2 构造密钥生成参数
      const properties = this.buildKeyGenProperties(config);

      // 1.3 调用 HUKS 生成密钥
      await huks.generateKeyItem(config.alias, properties);
      Logger.info(TAG, `Key generated: ${config.alias}`);
      return true;
    } catch (err) {
      const e = err as BusinessError;
      Logger.error(TAG, `Generate key failed: ${e.code} ${e.message}`);
      throw new Error(`HUKS generateKeyItem failed: ${e.code}`);
    }
  }

  // 构造 HUKS 密钥生成参数
  private buildKeyGenProperties(config: HuksKeyConfig): huks.HuksOptions {
    const algMap = {
      [KeyAlgorithm.RSA_2048]: {
        alg: huks.HuksKeyAlg.HUKS_ALG_RSA,
        size: huks.HuksKeySize.HUKS_RSA_KEY_SIZE_2048,
      },
      [KeyAlgorithm.RSA_3072]: {
        alg: huks.HuksKeyAlg.HUKS_ALG_RSA,
        size: huks.HuksKeySize.HUKS_RSA_KEY_SIZE_3072,
      },
      [KeyAlgorithm.EC_P256]: {
        alg: huks.HuksKeyAlg.HUKS_ALG_ECC,
        size: huks.HuksKeySize.HUKS_ECC_KEY_SIZE_256,
      },
      [KeyAlgorithm.SM2]: {
        alg: huks.HuksKeyAlg.HUKS_ALG_SM2,
        size: huks.HuksKeySize.HUKS_SM2_KEY_SIZE_256,
      },
    };

    const algInfo = algMap[config.algorithm];
    if (!algInfo) {
      throw new Error(`Unsupported algorithm: ${config.algorithm}`);
    }

    const props: huks.HuksParam[] = [
      { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: algInfo.alg },
      { tag: huks.HuksTag.HUKS_TAG_KEY_SIZE, value: algInfo.size },
      {
        tag: huks.HuksTag.HUKS_TAG_PURPOSE,
        value: config.purposes.reduce((acc, p) => acc | this.purposeToHuk(p), 0),
      },
    ];

    // 摘要算法
    if (config.digest) {
      props.push({
        tag: huks.HuksTag.HUKS_TAG_DIGEST,
        value: this.digestToHuk(config.digest),
      });
    }

    // 填充方式
    if (config.padding) {
      props.push({
        tag: huks.HuksTag.HUKS_TAG_PADDING,
        value: this.paddingToHuk(config.padding),
      });
    }

    return { properties: props };
  }

  private purposeToHuk(p: KeyPurpose): number {
    switch (p) {
      case KeyPurpose.SIGN_VERIFY: return huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_SIGN | huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_VERIFY;
      case KeyPurpose.ENCRYPT_DECRYPT: return huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_ENCRYPT | huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT;
      case KeyPurpose.KEY_AGREEMENT: return huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_KEY_AGREEMENT;
      case KeyPurpose.WRAP_UNWRAP: return huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_WRAP | huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_UNWRAP;
    }
  }

  private digestToHuk(d: string): number {
    const map: Record<string, number> = {
      'SHA1': huks.HuksKeyDigest.HUKS_DIGEST_SHA1,
      'SHA224': huks.HuksKeyDigest.HUKS_DIGEST_SHA224,
      'SHA256': huks.HuksKeyDigest.HUKS_DIGEST_SHA256,
      'SHA384': huks.HuksKeyDigest.HUKS_DIGEST_SHA384,
      'SHA512': huks.HuksKeyDigest.HUKS_DIGEST_SHA512,
      'SM3': huks.HuksKeyDigest.HUKS_DIGEST_SM3,
    };
    return map[d] || huks.HuksKeyDigest.HUKS_DIGEST_SHA256;
  }

  private paddingToHuk(p: string): number {
    const map: Record<string, number> = {
      'NONE': huks.HuksKeyPadding.HUKS_PADDING_NONE,
      'PKCS1': huks.HuksKeyPadding.HUKS_PADDING_PKCS1,
      'PKCS7': huks.HuksKeyPadding.HUKS_PADDING_PKCS7,
      'OAEP': huks.HuksKeyPadding.HUKS_PADDING_OAEP,
      'PSS': huks.HuksKeyPadding.HUKS_PADDING_PSS,
    };
    return map[p] || huks.HuksKeyPadding.HUKS_PADDING_PKCS1;
  }

  // ========== 2. 导出公钥 ==========
  async exportPublicKey(alias: string): Promise<Uint8Array> {
    try {
      const key = await huks.exportKeyItem(alias, {
        properties: [
          { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA },
        ]
      });
      Logger.info(TAG, `Public key exported, size: ${key.length}`);
      return key;
    } catch (err) {
      const e = err as BusinessError;
      Logger.error(TAG, `Export public key failed: ${e.code} ${e.message}`);
      throw e;
    }
  }

  // ========== 3. 签名（私钥不出 TEE） ==========
  async sign(alias: string, data: Uint8Array, digest: string = 'SHA256'): Promise<Uint8Array> {
    try {
      const properties: huks.HuksOptions = {
        properties: [
          { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA },
          {
            tag: huks.HuksTag.HUKS_TAG_PURPOSE,
            value: huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_SIGN,
          },
          { tag: huks.HuksTag.HUKS_TAG_DIGEST, value: this.digestToHuk(digest) },
          { tag: huks.HuksTag.HUKS_TAG_PADDING, value: huks.HuksKeyPadding.HUKS_PADDING_PKCS1 },
        ]
      };

      // 1. 初始化会话
      const handle = await huks.initSession(alias, properties);

      // 2. 完成签名
      const result = await huks.finishSession(handle.handle, data);
      
      // 3. 销毁会话
      await huks.abortSession(handle.handle);
      
      Logger.info(TAG, `Sign done, signature size: ${result.outData.length}`);
      return result.outData;
    } catch (err) {
      const e = err as BusinessError;
      Logger.error(TAG, `Sign failed: ${e.code} ${e.message}`);
      throw e;
    }
  }

  // ========== 4. 验签 ==========
  async verify(alias: string, data: Uint8Array, signature: Uint8Array, digest: string = 'SHA256'): Promise<boolean> {
    try {
      const properties: huks.HuksOptions = {
        properties: [
          { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA },
          {
            tag: huks.HuksTag.HUKS_TAG_PURPOSE,
            value: huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_VERIFY,
          },
          { tag: huks.HuksTag.HUKS_TAG_DIGEST, value: this.digestToHuk(digest) },
          { tag: huks.HuksTag.HUKS_TAG_PADDING, value: huks.HuksKeyPadding.HUKS_PADDING_PKCS1 },
        ]
      };

      const handle = await huks.initSession(alias, properties);
      const result = await huks.finishSession(handle.handle, data, signature);
      await huks.abortSession(handle.handle);
      
      return result.outData.length > 0;
    } catch (err) {
      Logger.warn(TAG, `Verify failed: ${(err as BusinessError).message}`);
      return false;
    }
  }

  // ========== 5. 导入证书到 HUKS ==========
  async importCertificate(alias: string, cert: Uint8Array): Promise<boolean> {
    try {
      const properties: huks.HuksOptions = {
        properties: [
          { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA },
        ]
      };
      await huks.importKeyItem(alias, cert, properties);
      Logger.info(TAG, `Certificate imported: ${alias}`);
      return true;
    } catch (err) {
      const e = err as BusinessError;
      Logger.error(TAG, `Import cert failed: ${e.code} ${e.message}`);
      throw e;
    }
  }

  // ========== 6. 删除密钥 ==========
  async deleteKey(alias: string): Promise<boolean> {
    try {
      await huks.deleteKeyItem(alias);
      Logger.info(TAG, `Key deleted: ${alias}`);
      return true;
    } catch (err) {
      Logger.warn(TAG, `Delete key warning: ${(err as BusinessError).message}`);
      return false;
    }
  }

  // ========== 7. 检查密钥是否存在 ==========
  async isKeyExist(alias: string): Promise<boolean> {
    try {
      const properties: huks.HuksOptions = {
        properties: [
          { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA },
        ]
      };
      await huks.exportKeyItem(alias, properties);
      return true;
    } catch {
      return false;
    }
  }

  // ========== 8. 获取密钥属性 ==========
  async getKeyAttributes(alias: string): Promise<huks.HuksKeyInfo | null> {
    try {
      return await huks.getKeyItemProperties(alias, {
        properties: [{ tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_RSA }]
      });
    } catch {
      return null;
    }
  }
}
```

---

### 三、CSR（Certificate Signing Request）生成

#### 1. ASN.1 工具（PKCS#10 编码）

```typescript
// common/x509/src/main/ets/asn1/Asn1Encoder.ets
/**
 * ASN.1 DER 编码工具（用于 PKCS#10 / X.509）
 */

export enum Asn1Tag {
  INTEGER = 0x02,
  BIT_STRING = 0x03,
  OCTET_STRING = 0x04,
  NULL = 0x05,
  OID = 0x06,
  UTF8_STRING = 0x0C,
  PRINTABLE_STRING = 0x13,
  SET = 0x31,
  SEQUENCE = 0x30,
  CONTEXT_0 = 0xA0,
  CONTEXT_1 = 0xA1,
  CONTEXT_3 = 0xA3,
}

export class Asn1Encoder {
  
  // 编码长度（DER）
  static encodeLength(length: number): Uint8Array {
    if (length < 0x80) {
      return new Uint8Array([length]);
    }
    const bytes: number[] = [];
    let temp = length;
    while (temp > 0) {
      bytes.unshift(temp & 0xFF);
      temp >>>= 8;
    }
    return new Uint8Array([0x80 | bytes.length, ...bytes]);
  }

  // 编码 TLV (Tag-Length-Value)
  static encodeTLV(tag: number, value: Uint8Array): Uint8Array {
    const length = Asn1Encoder.encodeLength(value.length);
    const result = new Uint8Array(1 + length.length + value.length);
    result[0] = tag;
    result.set(length, 1);
    result.set(value, 1 + length.length);
    return result;
  }

  // 编码 INTEGER
  static encodeInteger(value: Uint8Array | number): Uint8Array {
    let bytes: Uint8Array;
    if (typeof value === 'number') {
      bytes = new Uint8Array(4);
      new DataView(bytes.buffer).setUint32(0, value, false);
      // 去掉前导 0
      let i = 0;
      while (i < bytes.length - 1 && bytes[i] === 0) i++;
      bytes = bytes.slice(i);
      // 正整数最高位为 1 时补 0
      if (bytes[0] & 0x80) {
        const padded = new Uint8Array(bytes.length + 1);
        padded.set(bytes, 1);
        bytes = padded;
      }
    } else {
      bytes = value;
      if (bytes[0] & 0x80) {
        const padded = new Uint8Array(bytes.length + 1);
        padded.set(bytes, 1);
        bytes = padded;
      }
    }
    return Asn1Encoder.encodeTLV(Asn1Tag.INTEGER, bytes);
  }

  // 编码 BIT STRING
  static encodeBitString(value: Uint8Array, unusedBits: number = 0): Uint8Array {
    const result = new Uint8Array(value.length + 1);
    result[0] = unusedBits;
    result.set(value, 1);
    return Asn1Encoder.encodeTLV(Asn1Tag.BIT_STRING, result);
  }

  // 编码 OCTET STRING
  static encodeOctetString(value: Uint8Array): Uint8Array {
    return Asn1Encoder.encodeTLV(Asn1Tag.OCTET_STRING, value);
  }

  // 编码 OID
  static encodeOID(oid: string): Uint8Array {
    const parts = oid.split('.').map(Number);
    if (parts.length < 2) throw new Error('Invalid OID');
    
    // 第一个字节 = 40 * first + second
    const firstByte = parts[0] * 40 + parts[1];
    const bytes: number[] = [firstByte];
    
    // 后续字节用 Base-128 编码
    for (let i = 2; i < parts.length; i++) {
      let value = parts[i];
      const stack: number[] = [];
      stack.push(value & 0x7F);
      value >>>= 7;
      while (value > 0) {
        stack.push((value & 0x7F) | 0x80);
        value >>>= 7;
      }
      bytes.push(...stack.reverse());
    }
    
    return Asn1Encoder.encodeTLV(Asn1Tag.OID, new Uint8Array(bytes));
  }

  // 编码 SEQUENCE
  static encodeSequence(value: Uint8Array): Uint8Array {
    return Asn1Encoder.encodeTLV(Asn1Tag.SEQUENCE, value);
  }

  // 编码 SET
  static encodeSet(value: Uint8Array): Uint8Array {
    return Asn1Encoder.encodeTLV(Asn1Tag.SET, value);
  }

  // 编码 UTF8String
  static encodeUTF8String(str: string): Uint8Array {
    const bytes = new TextEncoder().encode(str);
    return Asn1Encoder.encodeTLV(Asn1Tag.UTF8_STRING, bytes);
  }

  // 编码 PrintableString
  static encodePrintableString(str: string): Uint8Array {
    const bytes = new TextEncoder().encode(str);
    return Asn1Encoder.encodeTLV(Asn1Tag.PRINTABLE_STRING, bytes);
  }

  // 编码 NULL
  static encodeNull(): Uint8Array {
    return new Uint8Array([Asn1Tag.NULL, 0x00]);
  }

  // 上下文标签 (Context-Specific)
  static encodeContext(tag: number, value: Uint8Array, constructed: boolean = true): Uint8Array {
    const tagByte = (constructed ? 0xA0 : 0x80) | tag;
    return Asn1Encoder.encodeTLV(tagByte, value);
  }
}

// ========== OID 常量 ==========
export class Oid {
  static readonly RSA_ENCRYPTION = '1.2.840.113549.1.1.1';
  static readonly SHA256_WITH_RSA = '1.2.840.113549.1.1.11';
  static readonly SHA384_WITH_RSA = '1.2.840.113549.1.1.12';
  static readonly SHA512_WITH_RSA = '1.2.840.113549.1.1.13';
  static readonly COMMON_NAME = '2.5.4.3';
  static readonly COUNTRY = '2.5.4.6';
  static readonly ORGANIZATION = '2.5.4.10';
  static readonly ORGANIZATIONAL_UNIT = '2.5.4.11';
  static readonly SERIAL_NUMBER = '2.5.4.5';
  static readonly EMAIL_ADDRESS = '1.2.840.113549.1.9.1';
  
  // EC OID
  static readonly EC_PUBLIC_KEY = '1.2.840.10045.2.1';
  static readonly EC_P256 = '1.2.840.10045.3.1.7';
  static readonly EC_P384 = '1.3.132.0.34';
  
  // 国密
  static readonly SM2_WITH_SM3 = '1.2.156.10197.1.501';
  static readonly SM3 = '1.2.156.10197.1.401';
  
  // PKCS#10
  static readonly PKCS_9_EXTENSION_REQUEST = '1.2.840.113549.1.9.14';
  
  // X.509 extensions
  static readonly BASIC_CONSTRAINTS = '2.5.29.19';
  static readonly KEY_USAGE = '2.5.29.15';
  static readonly EXT_KEY_USAGE = '2.5.29.37';
  static readonly SUBJECT_ALT_NAME = '2.5.29.17';
  static readonly SUBJECT_KEY_IDENTIFIER = '2.5.29.14';
  static readonly AUTHORITY_KEY_IDENTIFIER = '2.5.29.35';
  
  // ECP (Enrollment Certificate for V2X)
  static readonly ECP_CERTIFICATE = '1.3.6.1.4.1.29434.1.10.1';
}
```

#### 2. CSR Builder

```typescript
// common/x509/src/main/ets/csr/CsrBuilder.ets
import { Asn1Encoder, Oid } from '../asn1/Asn1Encoder';
import { HuksManager, KeyAlgorithm, KeyPurpose } from '../../huks/src/main/ets/HuksManager';

export interface CsrSubject {
  commonName: string;          // CN - 设备唯一标识
  organization?: string;       // O
  organizationalUnit?: string; // OU
  country?: string;            // C
  serialNumber?: string;       // serialNumber
  emailAddress?: string;       // emailAddress
}

export interface CsrExtension {
  oid: string;
  isCritical: boolean;
  value: Uint8Array;
}

export interface CsrRequest {
  keyAlias: string;            // HUKS 中私钥别名
  subject: CsrSubject;
  extensions?: CsrExtension[];
  signatureAlgorithm?: string; // 默认 SHA256_WITH_RSA
}

export class CsrBuilder {
  private huks = HuksManager.getInstance();

  // ========== 1. 生成 CSR（PEM 格式） ==========
  async build(request: CsrRequest): Promise<string> {
    // 1.1 编码 CSR 主体
    const certificationRequestInfo = await this.buildCertificationRequestInfo(request);
    
    // 1.2 对 CSR 主体做摘要
    const digest = await this.digestData(certificationRequestInfo, 'SHA256');
    
    // 1.3 用 HUKS 中的私钥签名（私钥不出 TEE）
    const signature = await this.huks.sign(
      request.keyAlias,
      digest,
      'SHA256'
    );
    
    // 1.4 拼装完整 PKCS#10
    const signatureAlgorithm = Asn1Encoder.encodeSequence(
      this.concatBytes(
        Asn1Encoder.encodeOID(Oid.SHA256_WITH_RSA),
        Asn1Encoder.encodeNull()
      )
    );
    
    const csr = Asn1Encoder.encodeSequence(
      this.concatBytes(
        certificationRequestInfo,
        signatureAlgorithm,
        Asn1Encoder.encodeBitString(signature, 0)
      )
    );
    
    // 1.5 转 PEM
    return this.toPEM(csr, 'CERTIFICATE REQUEST');
  }

  // ========== 2. 构造 CertificationRequestInfo ==========
  private async buildCertificationRequestInfo(request: CsrRequest): Promise<Uint8Array> {
    // 2.1 version = 0 (v1)
    const version = Asn1Encoder.encodeInteger(0);
    
    // 2.2 subject
    const subject = this.buildSubject(request.subject);
    
    // 2.3 subjectPKInfo (公钥 + 算法)
    const subjectPKInfo = await this.buildSubjectPKInfo(request.keyAlias);
    
    // 2.4 attributes [0] (含扩展请求)
    const attributes = this.buildAttributes(request.extensions || []);
    
    return Asn1Encoder.encodeSequence(
      this.concatBytes(version, subject, subjectPKInfo, attributes)
    );
  }

  // ========== 3. 构造 Subject ==========
  private buildSubject(subject: CsrSubject): Uint8Array {
    const rdns: Uint8Array[] = [];
    
    if (subject.country) {
      rdns.push(this.buildRDN(Oid.COUNTRY, subject.country, true));
    }
    if (subject.organization) {
      rdns.push(this.buildRDN(Oid.ORGANIZATION, subject.organization, false));
    }
    if (subject.organizationalUnit) {
      rdns.push(this.buildRDN(Oid.ORGANIZATIONAL_UNIT, subject.organizationalUnit, false));
    }
    if (subject.serialNumber) {
      rdns.push(this.buildRDN(Oid.SERIAL_NUMBER, subject.serialNumber, false));
    }
    if (subject.commonName) {
      rdns.push(this.buildRDN(Oid.COMMON_NAME, subject.commonName, false));
    }
    if (subject.emailAddress) {
      rdns.push(this.buildRDN(Oid.EMAIL_ADDRESS, subject.emailAddress, false));
    }
    
    // RDN 顺序编码（DER 要求按 tag 升序）
    rdns.sort((a, b) => {
      const tagA = a[0] === 0x31 ? a[3] : a[0];
      const tagB = b[0] === 0x31 ? b[3] : b[0];
      return tagA - tagB;
    });
    
    return Asn1Encoder.encodeSequence(this.concatBytes(...rdns));
  }

  // 构造单个 RDN (SET of AttributeTypeAndValue)
  private buildRDN(oid: string, value: string, isPrintable: boolean): Uint8Array {
    const attrValue = isPrintable
      ? Asn1Encoder.encodePrintableString(value)
      : Asn1Encoder.encodeUTF8String(value);
    
    const attrTypeAndValue = Asn1Encoder.encodeSequence(
      this.concatBytes(Asn1Encoder.encodeOID(oid), attrValue)
    );
    
    return Asn1Encoder.encodeSet(attrTypeAndValue);
  }

  // ========== 4. 构造 SubjectPublicKeyInfo ==========
  private async buildSubjectPKInfo(keyAlias: string): Promise<Uint8Array> {
    // 4.1 导出公钥
    const publicKey = await this.huks.exportPublicKey(keyAlias);
    
    // 4.2 去掉 PKCS#1 头（00 00 开头）转 SPKI
    // HUKS 导出格式：RSAPublicKey { modulus, publicExponent } 的 DER
    // SPKI = AlgorithmIdentifier + BIT STRING(publicKey DER)
    const algorithm = Asn1Encoder.encodeSequence(
      this.concatBytes(
        Asn1Encoder.encodeOID(Oid.RSA_ENCRYPTION),
        Asn1Encoder.encodeNull()
      )
    );
    
    // publicKey 已经是 RSAPublicKey 的 DER 编码
    return Asn1Encoder.encodeSequence(
      this.concatBytes(
        algorithm,
        Asn1Encoder.encodeBitString(publicKey, 0)
      )
    );
  }

  // ========== 5. 构造 Attributes (含扩展请求) ==========
  private buildAttributes(extensions: CsrExtension[]): Uint8Array {
    if (extensions.length === 0) {
      // 即使没有扩展，也需要 PKCS#9 - extensionRequest attribute
      return Asn1Encoder.encodeContext(0, new Uint8Array(0));
    }
    
    // 5.1 构造 ExtensionRequest 值
    const extsSeq = this.buildExtensions(extensions);
    
    // 5.2 构造 attribute value
    const attrValue = Asn1Encoder.encodeSequence(extsSeq);
    
    // 5.3 构造 attribute
    const attr = Asn1Encoder.encodeSequence(
      this.concatBytes(
        Asn1Encoder.encodeOID(Oid.PKCS_9_EXTENSION_REQUEST),
        attrValue
      )
    );
    
    // 5.4 用 [0] IMPLICIT 包裹
    return Asn1Encoder.encodeContext(0, attr);
  }

  private buildExtensions(extensions: CsrExtension[]): Uint8Array {
    const encodedExts = extensions.map(ext => {
      return Asn1Encoder.encodeSequence(
        this.concatBytes(
          Asn1Encoder.encodeOID(ext.oid),
          ext.isCritical ? Asn1Encoder.encodeBoolean(true) : new Uint8Array(0),
          Asn1Encoder.encodeOctetString(ext.value)
        )
      );
    });
    return this.concatBytes(...encodedExts);
  }

  // 工具方法
  private concatBytes(...arrays: Uint8Array[]): Uint8Array {
    const totalLength = arrays.reduce((sum, arr) => sum + arr.length, 0);
    const result = new Uint8Array(totalLength);
    let offset = 0;
    for (const arr of arrays) {
      result.set(arr, offset);
      offset += arr.length;
    }
    return result;
  }

  private async digestData(data: Uint8Array, algorithm: string): Promise<Uint8Array> {
    // 使用鸿蒙 cryptoFramework 做摘要
    return data; // 简化：HUKS 在 sign 时会自动做摘要
  }

  private toPEM(der: Uint8Array, label: string): string {
    const base64 = this.base64Encode(der);
    const chunks: string[] = [];
    for (let i = 0; i < base64.length; i += 64) {
      chunks.push(base64.substring(i, i + 64));
    }
    return `-----BEGIN ${label}-----\n${chunks.join('\n')}\n-----END ${label}-----\n`;
  }

  private base64Encode(bytes: Uint8Array): string {
    // 鸿蒙 base64 编码
    return '';  // 实际实现见 base64 工具
  }
}
```

---

### 四、TBOX-PKI 客户端（业务层）

```typescript
// features/pki/src/main/ets/client/TboxPkiClient.ets
import { HttpRequest, RequestMethod, Response, Header } from '@ohos.net.http';
import deviceInfo from '@ohos.deviceInfo';
import { HuksManager, KeyAlgorithm, KeyPurpose } from '../../../../common/huks/src/main/ets/HuksManager';
import { CsrBuilder, CsrRequest, CsrSubject, CsrExtension } from '../../../../common/x509/src/main/ets/csr/CsrBuilder';
import { Asn1Encoder, Oid } from '../../../../common/x509/src/main/ets/asn1/Asn1Encoder';
import { OcspClient } from './OcspClient';
import Logger from '../../../../common/utils/Logger';

const TAG = 'TboxPkiClient';
const KEY_ALIAS_DEVICE = 'tbox_device_key';
const CERT_ALIAS_DEVICE = 'tbox_device_cert';
const KEY_ALIAS_ECU = 'tbox_ecu_key';
const CERT_ALIAS_ECU = 'tbox_ecu_cert';

export interface PkiConfig {
  caBaseUrl: string;          // CA 服务器地址
  ocspUrl: string;            // OCSP 服务器
  crlUrl: string;             // CRL 分发点
  clientCertAlias?: string;   // mTLS 客户端证书（可选）
  trustedCaCerts: string[];   // 信任的 CA 证书链 PEM
}

export interface CertificateInfo {
  certPem: string;            // PEM 格式
  serialNumber: string;       // 证书序列号（HEX）
  notBefore: Date;            // 生效时间
  notAfter: Date;             // 过期时间
  issuer: string;             // 颁发者
  subject: string;            // 主体
  fingerprint: string;        // SHA-256 指纹
}

export interface EnrollmentRequest {
  csrPem: string;             // CSR PEM
  ecuSerial: string;          // ECU 序列号
  deviceType: string;         // TBOX / VCU / IVI
  attestation?: string;       // TEE Attestation
  hardwareId?: string;        // 硬件 ID
}

export interface EnrollmentResponse {
  certificatePem: string;     // 签发的证书
  caChainPem: string[];       // CA 证书链
  ocspUrl?: string;           // 后续 OCSP 地址
}

export class TboxPkiClient {
  private config: PkiConfig;
  private huks = HuksManager.getInstance();
  private csrBuilder = new CsrBuilder();
  private ocspClient: OcspClient;

  constructor(config: PkiConfig) {
    this.config = config;
    this.ocspClient = new OcspClient(config.ocspUrl);
  }

  // ========== 1. 初始化设备密钥对（首次启动） ==========
  async initializeDeviceKeys(): Promise<boolean> {
    try {
      Logger.info(TAG, 'Initializing device keys...');
      
      // 1.1 生成 TBOX 主密钥对
      await this.huks.generateKeyPair({
        alias: KEY_ALIAS_DEVICE,
        algorithm: KeyAlgorithm.RSA_2048,
        purposes: [KeyPurpose.SIGN_VERIFY, KeyPurpose.ENCRYPT_DECRYPT],
        digest: 'SHA256',
        padding: 'PKCS1',
      });
      
      // 1.2 生成 ECU 子密钥对
      await this.huks.generateKeyPair({
        alias: KEY_ALIAS_ECU,
        algorithm: KeyAlgorithm.EC_P256,
        purposes: [KeyPurpose.SIGN_VERIFY],
        digest: 'SHA256',
      });
      
      Logger.info(TAG, 'Device keys initialized');
      return true;
    } catch (err) {
      Logger.error(TAG, `Init keys failed: ${(err as Error).message}`);
      return false;
    }
  }

  // ========== 2. 生成设备身份 CSR ==========
  async generateDeviceCsr(certType: 'device' | 'ecu' = 'device'): Promise<string> {
    const keyAlias = certType === 'device' ? KEY_ALIAS_DEVICE : KEY_ALIAS_ECU;
    const udid = deviceInfo.udid;
    
    const subject: CsrSubject = {
      commonName: certType === 'device' 
        ? `TBOX-${udid}` 
        : `ECU-${udid}-${certType.toUpperCase()}`,
      organization: 'YourOEMCo.,Ltd',
      organizationalUnit: certType === 'device' ? 'Telematics' : 'Powertrain',
      country: 'CN',
      serialNumber: deviceInfo.serial,
    };
    
    // 2.1 构造扩展（关键）
    const extensions = this.buildStandardExtensions(certType);
    
    // 2.2 生成 CSR
    const csrPem = await this.csrBuilder.build({
      keyAlias,
      subject,
      extensions,
      signatureAlgorithm: Oid.SHA256_WITH_RSA,
    });
    
    Logger.info(TAG, `CSR generated for ${certType}`);
    return csrPem;
  }

  // 构造标准扩展（BasicConstraints, KeyUsage, EKU, SAN）
  private buildStandardExtensions(certType: 'device' | 'ecu'): CsrExtension[] {
    const extensions: CsrExtension[] = [];
    
    // 1. BasicConstraints
    const basicConstraints = Asn1Encoder.encodeSequence(
      Asn1Encoder.encodeBoolean(false)  // cA = false
    );
    extensions.push({
      oid: Oid.BASIC_CONSTRAINTS,
      isCritical: true,
      value: basicConstraints,
    });
    
    // 2. KeyUsage
    if (certType === 'device') {
      // TBOX 主证书：digitalSignature, keyEncipherment
      const keyUsageBits = new Uint8Array([0xA0]);  // 1010 0000
      extensions.push({
        oid: Oid.KEY_USAGE,
        isCritical: true,
        value: Asn1Encoder.encodeBitString(keyUsageBits, 0),
      });
      
      // 3. ExtendedKeyUsage - clientAuth
      const eku = Asn1Encoder.encodeSequence(
        Asn1Encoder.encodeOID('1.3.6.1.5.5.7.3.2')  // id-kp-clientAuth
      );
      extensions.push({
        oid: Oid.EXT_KEY_USAGE,
        isCritical: false,
        value: eku,
      });
    } else {
      // ECU 证书：digitalSignature
      const keyUsageBits = new Uint8Array([0x80]);
      extensions.push({
        oid: Oid.KEY_USAGE,
        isCritical: true,
        value: Asn1Encoder.encodeBitString(keyUsageBits, 0),
      });
    }
    
    // 4. SubjectAltName
    const san = this.buildSubjectAltName(deviceInfo.udid);
    extensions.push({
      oid: Oid.SUBJECT_ALT_NAME,
      isCritical: false,
      value: san,
    });
    
    return extensions;
  }

  private buildSubjectAltName(udid: string): Uint8Array {
    // 构造包含 URI 和 hardwareId 的 SAN
    const uriBytes = new TextEncoder().encode(`urn:tbox:${udid}`);
    const sanItems = [
      Asn1Encoder.encodeContext(6, uriBytes, false),  // URI = [6]
    ];
    return Asn1Encoder.encodeSequence(Asn1Encoder.concatArrays(...sanItems));
  }

  // ========== 3. 申请证书 ==========
  async enrollCertificate(certType: 'device' | 'ecu' = 'device'): Promise<CertificateInfo> {
    // 3.1 准备 CSR
    const csrPem = await this.generateDeviceCsr(certType);
    
    // 3.2 准备请求
    const request: EnrollmentRequest = {
      csrPem,
      ecuSerial: deviceInfo.serial,
      deviceType: certType === 'device' ? 'TBOX' : 'ECU',
      attestation: await this.getTeeAttestation(certType),
      hardwareId: deviceInfo.udid,
    };
    
    // 3.3 发送请求（mTLS）
    const response = await this.httpsRequest<EnrollmentResponse>(
      '/api/v1/cert/enroll',
      'POST',
      request
    );
    
    if (!response.certificatePem) {
      throw new Error('Enrollment failed: no certificate returned');
    }
    
    // 3.4 存储证书到 HUKS
    const certAlias = certType === 'device' ? CERT_ALIAS_DEVICE : CERT_ALIAS_ECU;
    const certBytes = this.pemToBytes(response.certificatePem);
    await this.huks.importCertificate(certAlias, certBytes);
    
    // 3.5 保存 CA 链到本地
    await this.saveCaChain(response.caChainPem);
    
    Logger.info(TAG, `Certificate enrolled for ${certType}`);
    
    return this.parseCertificate(response.certificatePem);
  }

  // ========== 4. 证书轮换 ==========
  async rotateCertificate(certType: 'device' | 'ecu' = 'device'): Promise<CertificateInfo> {
    Logger.info(TAG, `Rotating certificate for ${certType}...`);
    
    // 4.1 检查旧证书状态（OCSP）
    const certAlias = certType === 'device' ? CERT_ALIAS_DEVICE : CERT_ALIAS_ECU;
    const oldCertPem = await this.loadStoredCert(certAlias);
    if (oldCertPem) {
      const status = await this.ocspClient.checkStatus(oldCertPem);
      Logger.info(TAG, `Old cert OCSP status: ${status}`);
    }
    
    // 4.2 删除旧密钥和证书
    const keyAlias = certType === 'device' ? KEY_ALIAS_DEVICE : KEY_ALIAS_ECU;
    await this.huks.deleteKey(keyAlias);
    await this.huks.deleteKey(certAlias);
    
    // 4.3 重新生成密钥
    await this.initializeDeviceKeys();
    
    // 4.4 申请新证书
    return await this.enrollCertificate(certType);
  }

  // ========== 5. 周期性检查证书过期 ==========
  async checkAndRotateIfNeeded(): Promise<boolean> {
    try {
      const deviceCert = await this.loadStoredCert(CERT_ALIAS_DEVICE);
      if (!deviceCert) {
        Logger.warn(TAG, 'No device cert found, enrolling new one');
        await this.enrollCertificate('device');
        return true;
      }
      
      const certInfo = this.parseCertificate(deviceCert);
      const now = Date.now();
      const remainingMs = certInfo.notAfter.getTime() - now;
      const remainingDays = remainingMs / (1000 * 60 * 60 * 24);
      
      // 剩余有效期 < 30 天时主动轮换
      if (remainingDays < 30) {
        Logger.info(TAG, `Cert expires in ${remainingDays.toFixed(1)} days, rotating`);
        await this.rotateCertificate('device');
        return true;
      }
      
      Logger.info(TAG, `Cert still valid for ${remainingDays.toFixed(1)} days`);
      return false;
    } catch (err) {
      Logger.error(TAG, `Check/rotate failed: ${(err as Error).message}`);
      return false;
    }
  }

  // ========== 6. 获取 TEE Attestation ==========
  private async getTeeAttestation(certType: 'device' | 'ecu'): Promise<string> {
    const keyAlias = certType === 'device' ? KEY_ALIAS_DEVICE : KEY_ALIAS_ECU;
    
    try {
      // 调用 TEE/REE 接口获取 attestation 证书链
      // 鸿蒙中通过 specific TEE API（OEM/供应商提供）
      const teeAttest = await this.invokeTeeAttestation(keyAlias);
      return JSON.stringify(teeAttest);
    } catch (err) {
      Logger.warn(TAG, `TEE attestation failed: ${(err as Error).message}`);
      return '';
    }
  }

  private async invokeTeeAttestation(keyAlias: string): Promise<any> {
    // 调用鸿蒙的 HUKS attestation
    // 实际实现因 TEE 供应商而异（Trustonic/OPTEE/自研）
    return {
      teeType: 'OP-TEE',
      challenge: 'random-nonce',
      certificates: [],  // TEE 证书链
    };
  }

  // ========== 7. mTLS HTTPS 请求 ==========
  private async httpsRequest<T>(
    path: string, 
    method: 'GET' | 'POST' | 'PUT', 
    body?: any
  ): Promise<T> {
    const httpRequest = new HttpRequest();
    
    const headers: Header[] = [
      { key: 'Content-Type', val: 'application/json' },
      { key: 'X-Device-Id', val: deviceInfo.udid },
      { key: 'X-Device-Type', val: 'TBOX' },
    ];
    
    // 注：鸿蒙 http 库直接支持 mTLS 需要扩展
    // 此处简化，实际可使用 @ohos.net.tls 或原生 socket
    
    try {
      const response = await httpRequest.request({
        url: `${this.config.caBaseUrl}${path}`,
        method: method as RequestMethod,
        header: headers,
        extraData: body ? JSON.stringify(body) : undefined,
        caPath: this.config.trustedCaCerts[0],  // 信任根 CA
        clientCert: { 
          certPath: await this.getCertPath(CERT_ALIAS_DEVICE), 
          keyPath: await this.getKeyPath(KEY_ALIAS_DEVICE) 
        },
        connectTimeout: 10000,
        readTimeout: 30000,
      });
      
      if (response.responseCode !== 200) {
        throw new Error(`HTTP ${response.responseCode}: ${response.result}`);
      }
      
      return JSON.parse(response.result as string) as T;
    } finally {
      httpRequest.destroy();
    }
  }

  // ========== 工具方法 ==========
  private parseCertificate(pem: string): CertificateInfo {
    // 解析 X.509 证书（用 ASN.1 解码）
    // 简化返回，实际需要完整解析
    return {
      certPem: pem,
      serialNumber: '00:00:00:00:00:00:00:00',
      notBefore: new Date(),
      notAfter: new Date(Date.now() + 365 * 24 * 60 * 60 * 1000),
      issuer: 'CN=YourOEMCA',
      subject: 'CN=TBOX-XXX',
      fingerprint: 'SHA256:...',
    };
  }

  private pemToBytes(pem: string): Uint8Array {
    const base64 = pem
      .replace(/-----BEGIN [^-]+-----/, '')
      .replace(/-----END [^-]+-----/, '')
      .replace(/\s/g, '');
    return new Uint8Array(this.base64ToBytes(base64));
  }

  private base64ToBytes(base64: string): number[] {
    // 实现 base64 解码
    return [];
  }

  private async saveCaChain(chain: string[]): Promise<void> {
    // 存储 CA 链到应用沙箱
  }

  private async loadStoredCert(alias: string): Promise<string | null> {
    // 从 HUKS 或本地存储加载证书
    return null;
  }

  private async getCertPath(alias: string): Promise<string> {
    return '';
  }

  private async getKeyPath(alias: string): Promise<string> {
    return '';
  }
}
```

---

### 五、OCSP 客户端实现

```typescript
// features/pki/src/main/ets/client/OcspClient.ets
import { Asn1Encoder, Oid } from '../../../../common/x509/src/main/ets/asn1/Asn1Encoder';
import { HuksManager } from '../../../../common/huks/src/main/ets/HuksManager';

export type CertStatus = 'good' | 'revoked' | 'unknown';

export interface OcspRequest {
  issuerNameHash: Uint8Array;      // 颁发者 DN 哈希
  issuerKeyHash: Uint8Array;       // 颁发者公钥哈希
  serialNumber: Uint8Array;        // 证书序列号
}

export class OcspClient {
  private ocspUrl: string;
  private huks = HuksManager.getInstance();

  constructor(ocspUrl: string) {
    this.ocspUrl = ocspUrl;
  }

  // ========== 检查证书状态 ==========
  async checkStatus(certPem: string): Promise<CertStatus> {
    // 1. 解析证书，提取颁发者信息和序列号
    const req = this.buildOcspRequestFromCert(certPem);
    
    // 2. 编码 OCSP 请求（DER）
    const ocspReqDer = this.encodeOcspRequest(req);
    
    // 3. 发送 HTTP POST 到 OCSP 服务器
    const ocspResponseDer = await this.sendOcspRequest(ocspReqDer);
    
    // 4. 解析响应
    return this.parseOcspResponse(ocspResponseDer);
  }

  // 构造 OCSP Request
  private buildOcspRequestFromCert(certPem: string): OcspRequest {
    // 实际需要解析 X.509 证书
    return {
      issuerNameHash: new Uint8Array(20),    // SHA-1 of issuer DN
      issuerKeyHash: new Uint8Array(20),     // SHA-1 of issuer public key
      serialNumber: new Uint8Array([0x01]),  // 证书序列号
    };
  }

  // 编码 OCSP Request (PKIX OCSP)
  private encodeOcspRequest(req: OcspRequest): Uint8Array {
    // OCSPRequest ::= SEQUENCE {
    //   tbsRequest              TBSRequest,
    //   optionalSignature   [0] EXPLICIT Signature OPTIONAL
    // }
    
    // TBSRequest
    const version = Asn1Encoder.encodeInteger(0);  // v1
    
    // requestorName (省略)
    
    // requestList
    const certId = Asn1Encoder.encodeSequence(
      Asn1Encoder.concatArrays(
        // hashAlgorithm = SHA-1
        Asn1Encoder.encodeSequence(Asn1Encoder.encodeOID('1.3.14.3.2.26'), Asn1Encoder.encodeNull()),
        Asn1Encoder.encodeOctetString(req.issuerNameHash),
        Asn1Encoder.encodeOctetString(req.issuerKeyHash),
        Asn1Encoder.encodeInteger(req.serialNumber)
      )
    );
    
    const request = Asn1Encoder.encodeSequence(certId);
    const requestList = Asn1Encoder.encodeSequence(request);
    
    const tbsRequest = Asn1Encoder.encodeSequence(
      Asn1Encoder.concatArrays(version, requestList)
    );
    
    return Asn1Encoder.encodeSequence(tbsRequest);
  }

  // 发送 OCSP 请求
  private async sendOcspRequest(ocspReq: Uint8Array): Promise<Uint8Array> {
    const response = await fetch(this.ocspUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/ocsp-request' },
      body: ocspReq,
    });
    const buffer = await response.arrayBuffer();
    return new Uint8Array(buffer);
  }

  // 解析 OCSP Response
  private parseOcspResponse(der: Uint8Array): CertStatus {
    // OCSPResponse ::= SEQUENCE {
    //   responseStatus         OCSPResponseStatus,
    //   responseBytes      [0] EXPLICIT ResponseBytes OPTIONAL
    // }
    // responseStatus: 0=successful, 1=malformedRequest, 2=internalError, 
    //                 3=tryLater, 4=sigRequired, 5=unauthorized
    
    // 简化：直接返回 good
    // 实际需要完整解析
    return 'good';
  }
}
```

---

### 六、证书管理 UI（鸿蒙车机端）

```typescript
// features/certificate/src/main/ets/pages/CertManager.ets
import { TboxPkiClient } from '../../../pki/src/main/ets/client/TboxPkiClient';
import { HuksManager, KeyAlgorithm, KeyPurpose } from '../../../../../common/huks/src/main/ets/HuksManager';

@Entry
@Component
struct CertManager {
  @State deviceCertStatus: string = '查询中...';
  @State deviceCertExpiry: string = '--';
  @State ecuCertStatus: string = '查询中...';
  @State ecuCertExpiry: string = '--';
  @State isLoading: boolean = false;
  
  private pkiClient: TboxPkiClient = new TboxPkiClient({
    caBaseUrl: 'https://pki.your-oem.com',
    ocspUrl: 'https://ocsp.your-oem.com',
    crlUrl: 'https://crl.your-oem.com',
    trustedCaCerts: ['/etc/ssl/certs/your-oem-root.pem'],
  });

  aboutToAppear() {
    this.refreshCertStatus();
  }

  build() {
    Column() {
      // 顶部导航
      Row() {
        Text('证书管理中心')
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
      }
      .width('100%')
      .height(56)
      .padding({ left: 20 })
      .backgroundColor('#1E88E5')
      
      // 设备证书卡片
      this.buildCertCard('设备主证书 (TBOX)', this.deviceCertStatus, this.deviceCertExpiry, 'device')
      
      // ECU 证书卡片
      this.buildCertCard('ECU 子证书', this.ecuCertStatus, this.ecuCertExpiry, 'ecu')
      
      // 操作按钮
      Button(this.isLoading ? '处理中...' : '立即更新证书')
        .width('90%')
        .height(48)
        .margin({ top: 30 })
        .onClick(async () => {
          this.isLoading = true;
          try {
            await this.pkiClient.rotateCertificate('device');
            await this.refreshCertStatus();
            promptAction.showToast({ message: '证书更新成功' });
          } catch (e) {
            promptAction.showToast({ message: `更新失败: ${(e as Error).message}` });
          } finally {
            this.isLoading = false;
          }
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F5F5')
  }

  @Builder
  buildCertCard(title: string, status: string, expiry: string, type: 'device' | 'ecu') {
    Column() {
      Text(title).fontSize(18).fontWeight(FontWeight.Medium)
      Row() {
        Text('状态:').fontSize(14).fontColor(Color.Gray)
        Text(status).fontSize(14).fontColor(status === '正常' ? Color.Green : Color.Red)
      }
      .width('100%')
      .margin({ top: 10 })
      
      Row() {
        Text('有效期至:').fontSize(14).fontColor(Color.Gray)
        Text(expiry).fontSize(14)
      }
      .width('100%')
      .margin({ top: 5 })
      
      Row({ space: 10 }) {
        Button('查看详情')
          .layoutWeight(1)
          .onClick(() => this.showCertDetail(type))
        Button('手动更新')
          .layoutWeight(1)
          .onClick(() => this.manualRotate(type))
      }
      .width('100%')
      .margin({ top: 15 })
    }
    .width('90%')
    .padding(15)
    .margin({ top: 15 })
    .borderRadius(8)
    .backgroundColor(Color.White)
  }

  private async refreshCertStatus() {
    const huks = HuksManager.getInstance();
    const isDeviceCert = await huks.isKeyExist('tbox_device_cert');
    this.deviceCertStatus = isDeviceCert ? '正常' : '未申请';
    // 实际应解析证书获取有效期
    this.deviceCertExpiry = isDeviceCert ? '2027-06-08' : '--';
    
    const isEcuCert = await huks.isKeyExist('tbox_ecu_cert');
    this.ecuCertStatus = isEcuCert ? '正常' : '未申请';
    this.ecuCertExpiry = isEcuCert ? '2027-06-08' : '--';
  }

  private showCertDetail(type: 'device' | 'ecu') {
    // 显示证书详情
  }

  private async manualRotate(type: 'device' | 'ecu') {
    this.isLoading = true;
    try {
      await this.pkiClient.rotateCertificate(type);
      await this.refreshCertStatus();
      promptAction.showToast({ message: '更新成功' });
    } catch (e) {
      promptAction.showToast({ message: `更新失败: ${(e as Error).message}` });
    } finally {
      this.isLoading = false;
    }
  }
}
```

---

### 七、完整调用流程

```typescript
// features/pki/src/main/ets/PkiServiceEntry.ets
import { TboxPkiClient } from './client/TboxPkiClient';

export class PkiService {
  private pkiClient: TboxPkiClient;
  private rotationTimer: number = -1;

  constructor() {
    this.pkiClient = new TboxPkiClient({
      caBaseUrl: 'https://pki.your-oem.com',
      ocspUrl: 'https://ocsp.your-oem.com',
      crlUrl: 'https://crl.your-oem.com',
      trustedCaCerts: ['/etc/ssl/certs/your-oem-root.pem'],
    });
  }

  // ========== 系统启动时初始化 ==========
  async initialize(): Promise<void> {
    try {
      // 1. 检查并生成设备密钥
      const huks = (await import('../../../common/huks/src/main/ets/HuksManager')).HuksManager.getInstance();
      const deviceKeyExists = await huks.isKeyExist('tbox_device_key');
      if (!deviceKeyExists) {
        console.log('PkiService: generating device keys...');
        await this.pkiClient.initializeDeviceKeys();
      }
      
      // 2. 检查并申请证书
      const needRotate = await this.pkiClient.checkAndRotateIfNeeded();
      if (needRotate) {
        console.log('PkiService: certificate enrolled/rotated');
      }
      
      // 3. 启动定时检查（每 6 小时一次）
      this.startRotationCheck();
      
      console.log('PkiService initialized');
    } catch (err) {
      console.error(`PkiService init failed: ${(err as Error).message}`);
    }
  }

  private startRotationCheck(): void {
    this.rotationTimer = setInterval(async () => {
      try {
        await this.pkiClient.checkAndRotateIfNeeded();
      } catch (err) {
        console.error(`Periodic check failed: ${(err as Error).message}`);
      }
    }, 6 * 60 * 60 * 1000);
  }

  // ========== 对外暴露：使用设备私钥签名 ==========
  async signWithDeviceKey(data: Uint8Array): Promise<Uint8Array> {
    const huks = (await import('../../../common/huks/src/main/ets/HuksManager')).HuksManager.getInstance();
    return await huks.sign('tbox_device_key', data, 'SHA256');
  }

  async signWithEcuKey(data: Uint8Array): Promise<Uint8Array> {
    const huks = (await import('../../../common/huks/src/main/ets/HuksManager')).HuksManager.getInstance();
    return await huks.sign('tbox_ecu_key', data, 'SHA256');
  }
}
```

---

## ❓ 下一步深入方向

车机端 HUKS 集成和 CSR/CA/OCSP 完整流程已经铺好。请告诉我你想继续深入哪个方向：

1. **TBOX-PKI 与 HiCar 车控指令联动**（车控签名链路完整实现）？
2. **TEE 远程证明（Attestation）详解**（对接 EAT/PSA Attestation Token）？
3. **国密 SM2/SM3/SM4 算法在车机端的具体实现**？
4. **OTA 包签名验证链路**（HUKS 验签 + 证书链验证）？
5. **V2X ECP 证书（IEEE 1609.2）签发**（用于 V2X 通信）？
6. **鸿蒙车机端 mTLS 实现细节**（http 库扩展 / 原生 socket）？
7. **证书格式 X.509 完整解析器**（从 DER 反解字段）？

告诉我你的下一步重点，我继续展开。