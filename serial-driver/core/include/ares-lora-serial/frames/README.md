# Notes on Frame Payloads

1. Each frame payload structure is organized into a type class. These type classes are described by the folder names.
2. Type classes may have subclasses
3. Lora payload structs must be tagged with the LoraBase
4. Necessary Lora messages that expect a response must have a response type associated with the payload struct.
