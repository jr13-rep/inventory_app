# inventory_app
This project is a native Windows C++ application that builds into a single `.exe`.

### Prerequisites

1. Install Visual Studio 2022 (Desktop development with C++) or the Build Tools, including the Windows SDK.
2. Install [vcpkg](https://github.com/microsoft/vcpkg) and the static SQLite package:

   ```powershell
   git clone https://github.com/microsoft/vcpkg
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install sqlite3:x64-windows-static
   ```

### Build steps (Developer Command Prompt)

```bat
cd path\to\python_database
cl /std:c++20 /EHsc /O2 /MT main.cpp ^
  /I %VCPKG_ROOT%\installed\x64-windows-static\include ^
  /link /LIBPATH:%VCPKG_ROOT%\installed\x64-windows-static\lib ^
  sqlite3.lib comctl32.lib
```

The executable `main.exe` will be created in the current folder. You can rename it to
`InventoryApp.exe` if desired.

### Build steps (Visual Studio IDE)

1. Open Visual Studio 2022.
2. Select **Create a new project** → **Console App** (C++).
3. Name the project `InventoryApp`, choose a location, and click **Create**.
4. Replace the generated `InventoryApp.cpp` content with the contents of `main.cpp`.
5. Open **Project** → **Properties**:
   - **Configuration**: `Release`
   - **Platform**: `x64`
6. In **Configuration Properties** → **C/C++** → **General**:
   - **Additional Include Directories**:  
     `%VCPKG_ROOT%\installed\x64-windows-static\include`
7. In **Configuration Properties** → **Linker** → **General**:
   - **Additional Library Directories**:  
     `%VCPKG_ROOT%\installed\x64-windows-static\lib`
8. In **Configuration Properties** → **Linker** → **Input**:
   - **Additional Dependencies**:  
     `sqlite3.lib;comctl32.lib`
9. Click **OK** to save the settings.
10. Build with **Build** → **Build Solution**.
11. The executable will be at:
    - `InventoryApp\x64\Release\InventoryApp.exe`

## Windows validation checklist

To validate on Windows, run the following steps on a Windows machine:

1. Build the executable using the steps above.
2. Run the application and verify you can:
   - Enter Name, Part Number, NSN, Serial Number, and Quantity then click **Save**.
   - Select a row to populate the fields and click **Update** to edit it.
   - Select a row and click **Delete** to remove it.
   - Search using any field and see matching results in the table.
   - Enter Name, Part Number, NSN, Serial Number, and Quantity then click **Save**.
   - Select a row to populate the fields and click **Update** to edit it.
   - Select a row and click **Delete** to remove it.
   - Search using any field and see matching results in the table.
