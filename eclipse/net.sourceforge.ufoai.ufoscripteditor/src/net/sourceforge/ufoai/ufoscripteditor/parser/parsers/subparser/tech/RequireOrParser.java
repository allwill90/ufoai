package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class RequireOrParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "require_OR";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new RequireOrParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public RequireOrParser(IParserContext ctx) {
		super(ctx);
	}
}
