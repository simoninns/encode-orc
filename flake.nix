{
  description = "encode-orc - LaserDisc and tape video encoder";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        encode-orc = pkgs.stdenv.mkDerivation {
          pname = "encode-orc";
          version = "1.0.0";

          src = ./.;

          strictDeps = true;
          cmakeBuildDir = "build";
          cmakeBuildType = "Release";

          nativeBuildInputs = with pkgs; [
            cmake
            ccache
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

          postInstall = ''
            wrapProgram $out/bin/encode-orc \
              --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.ffmpeg ]} \
              --set ENCODE_ORC_ASSETS $out/share/encode-orc/assets
          '';

          meta = with pkgs.lib; {
            description = "encode-orc - LaserDisc and tape video encoder";
            homepage = "https://github.com/simoninns/encode-orc";
            license = licenses.gpl3Plus;
            platforms = platforms.linux ++ platforms.darwin;
            mainProgram = "encode-orc";
          };
        };
      in
      {
        packages.encode-orc = encode-orc;
        packages.default = encode-orc;

        checks.encode-orc-tests = encode-orc.overrideAttrs (_old: {
          doCheck = true;
          checkPhase = ''
            export PATH=${pkgs.lib.makeBinPath [ pkgs.ffmpeg ]}:$PATH
            ctest --progress --output-on-failure
          '';
          installPhase = ''
            mkdir -p $out
          '';
        });

        devShells.default = pkgs.mkShell {
          inputsFrom = [ encode-orc ];
        };

        apps.default = {
          type = "app";
          program = "${encode-orc}/bin/encode-orc";
          meta = encode-orc.meta;
        };

        formatter = pkgs.nixpkgs-fmt;
      }
    );
}
