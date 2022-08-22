import { ErrorCode, Response } from "proto/api_pb";

interface BinaryDeserializable<T> {
  deserializeBinary(bytes: Uint8Array): T;
}

export interface BinarySerializable {
  serializeBinary(): Uint8Array;
}

export class EmptyPayload {
  bytes?: Uint8Array;

  serializeBinary(): Uint8Array {
    return this.bytes ?? new Uint8Array();
  }

  static deserializeBinary(bytes: Uint8Array): EmptyPayload {
    const res = new EmptyPayload();
    if (bytes.length) {
      res.bytes = bytes;
    }
    return res;
  }
}

export async function request<T>(
  resType: BinaryDeserializable<T>,
  url: string,
  req?: BinarySerializable
): Promise<[number, (T | Uint8Array)?]> {
  try {
    const result = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/x-protobuf" },
      body: req?.serializeBinary(),
    });
    const buffer = new Uint8Array(await result.arrayBuffer());
    if (result.status !== 200) {
      return [-result.status];
    }
    const response = Response.deserializeBinary(buffer);
    if (response.getCode() !== ErrorCode.OK) {
      return [response.getCode() || -result.status, response.getResponse_asU8()];
    } else {
      return [ErrorCode.OK, resType.deserializeBinary(response.getResponse_asU8())];
    }
  } catch (e) {
    return [-1000];
  }
}

export async function requestU<T>(
  resType: BinaryDeserializable<T>,
  url: string,
  req?: BinarySerializable
): Promise<T> {
  const [code, res] = await request(resType, url, req);
  if (code !== ErrorCode.OK || res === undefined) {
    throw new Error(`Failed to request ${url}, got code=${code}`);
  } else {
    return res as T;
  }
}
