// swift-tools-version:5.6

import Foundation
import PackageDescription

let dir = Context.packageDirectory
// Three languages: NOVA text (src/), the schema language (schema/src/) and
// NOVA Lines (lines/src/, no external scanner).
var sources = ["src/parser.c", "schema/src/parser.c", "lines/src/parser.c"]
for scanner in ["src/scanner.c", "schema/src/scanner.c", "lines/src/scanner.c"] {
    if FileManager.default.fileExists(atPath: "\(dir)/\(scanner)") {
        sources.append(scanner)
    }
}

let package = Package(
    name: "TreeSitterNova",
    products: [
        .library(name: "TreeSitterNova", targets: ["TreeSitterNova"]),
    ],
    dependencies: [
        .package(url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.10.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterNova",
            dependencies: [],
            path: ".",
            sources: sources,
            resources: [
                .copy("queries"),
                .copy("schema/queries"),
                .copy("lines/queries"),
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterNovaTests",
            dependencies: [
                .product(name: "SwiftTreeSitter", package: "swift-tree-sitter"),
                "TreeSitterNova",
            ],
            path: "bindings/swift/TreeSitterNovaTests"
        )
    ],
    cLanguageStandard: .c11
)
