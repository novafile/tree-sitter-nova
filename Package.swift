// swift-tools-version:5.6

import Foundation
import PackageDescription

let dir = Context.packageDirectory
// Both languages: NOVA text (src/) and the schema language (schema/src/).
var sources = ["src/parser.c", "schema/src/parser.c"]
for scanner in ["src/scanner.c", "schema/src/scanner.c"] {
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
