import XCTest
import SwiftTreeSitter
import TreeSitterNova

final class TreeSitterNovaTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_nova())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading NOVA grammar")
    }

    func testCanLoadSchemaGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_nova_schema())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading NOVA schema grammar")
    }
}
