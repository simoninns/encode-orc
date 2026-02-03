{
  description = "encode-orc - LaserDisc video encoder";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "encode-orc";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            makeWrapper
          ];

          buildInputs = with pkgs; [
            spdlog
            sqlite
            yaml-cpp
            libpng
            ffmpeg
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          postInstall = ''
            wrapProgram $out/bin/encode-orc \
              --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.ffmpeg ]}
          '';

          meta = with pkgs.lib; {
            description = "LaserDisc video encoder for generating TBC files";
            homepage = "https://github.com/simoninns/encode-orc";
            license = licenses.gpl3Plus;
            platforms = platforms.linux ++ platforms.darwin;
            mainProgram = "encode-orc";
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Build tools
            cmake
            pkg-config
            
            # Dependencies
            spdlog
            sqlite
            yaml-cpp
            libpng
            ffmpeg
            
            # Development tools
            gdb
            clang-tools
            ccache
          ];

          shellHook = ''
            echo "encode-orc development environment"
            echo "Run 'cmake -B build -S .' to configure"
            echo "Run 'cmake --build build' to build"
          '';
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/encode-orc";
        };
      }
    );
}
