{
    description = "Coldwrite development environment";

    inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    outputs = { nixpkgs, ... }:
      let
        system = "x86_64-linux";
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        devShells.${system}.default = pkgs.mkShell {
          strictDeps = true;

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            wayland
            libxkbcommon
            wayland-protocols
          ];
        };
      };
}
