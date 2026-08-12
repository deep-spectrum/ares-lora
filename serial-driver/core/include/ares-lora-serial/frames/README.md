# Notes on Frame Payloads

1. Each frame payload structure is organized into a type class. These type classes are described by the folder names.
2. Type classes may have subclasses
3. The field names `id` and `broadcast` are reserved specifically for the lora type class. Do not use them for any other classes.
4. Lora payload structs must be tagged with the LoraBase
